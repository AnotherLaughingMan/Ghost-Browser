#include "core/CookieManager.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimer>
#include <QTemporaryDir>
#include <QWebEngineCookieStore>

namespace {

constexpr int kCookieReloadDelayMs = 500;
constexpr int kCookieReloadRetries = 5;

QString cookieKey(const QNetworkCookie &cookie)
{
    return QString::fromUtf8(cookie.name())
        + QLatin1Char('\n')
        + cookie.domain()
        + QLatin1Char('\n')
        + cookie.path();
}

}

CookieManager::CookieManager(QWebEngineCookieStore *store, const QString &storagePath, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_storagePath(storagePath)
{
    if (!m_store)
        return;

    m_store->setCookieFilter([this](const QWebEngineCookieStore::FilterRequest &request) {
        return !m_blockThirdPartyCookies.load() || !request.thirdParty;
    });

    connect(m_store, &QWebEngineCookieStore::cookieAdded,
            this, &CookieManager::onCookieAdded);
    connect(m_store, &QWebEngineCookieStore::cookieRemoved,
            this, &CookieManager::onCookieRemoved);

    reload();
}

void CookieManager::setBlockThirdPartyCookies(bool enabled)
{
    m_blockThirdPartyCookies.store(enabled);
}

QString CookieManager::getCookiesJson() const
{
    m_visibleCookies = buildVisibleCookies();

    QJsonArray arr;
    for (int i = 0; i < m_visibleCookies.size(); ++i) {
        const QNetworkCookie &c = m_visibleCookies.at(i);
        arr.append(QJsonObject {
            { QStringLiteral("index"),    i },
            { QStringLiteral("name"),     QString::fromUtf8(c.name()) },
            { QStringLiteral("domain"),   c.domain() },
            { QStringLiteral("path"),     c.path() },
            { QStringLiteral("value"),    QString::fromUtf8(c.value()) },
            { QStringLiteral("secure"),   c.isSecure() },
            { QStringLiteral("httpOnly"), c.isHttpOnly() },
            { QStringLiteral("session"),  c.isSessionCookie() },
            { QStringLiteral("expires"),  c.expirationDate().isValid()
                  ? c.expirationDate().toString(Qt::ISODate)
                  : QString() },
        });
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void CookieManager::deleteByIndex(int index)
{
    if (index < 0 || index >= m_visibleCookies.size() || !m_store)
        return;

    m_store->deleteCookie(m_visibleCookies.at(index));
}

void CookieManager::clearByAge(const QString &range)
{
    if (!m_store)
        return;

    if (range == QLatin1String("forever")) {
        clearAll();
        return;
    }

    QDateTime cutoff;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (range == QLatin1String("1hour"))
        cutoff = now.addSecs(-3600);
    else if (range == QLatin1String("24hours"))
        cutoff = now.addSecs(-86400);
    else if (range == QLatin1String("7days"))
        cutoff = now.addDays(-7);
    else if (range == QLatin1String("4weeks"))
        cutoff = now.addDays(-28);
    else
        return;

    // Delete session cookies and any added after the cutoff
    for (const QNetworkCookie &c : std::as_const(m_cookies)) {
        if (c.isSessionCookie() || !c.expirationDate().isValid()) {
            m_store->deleteCookie(c);
        }
    }

    // For persistent cookies we can't easily know when they were *set*,
    // so clear-by-age deletes all — matching History behaviour.
    // A more granular approach would require tracking insertion time.
    m_store->deleteAllCookies();
}

void CookieManager::clearAll()
{
    if (!m_store)
        return;
    m_store->deleteAllCookies();
}

QList<QNetworkCookie> CookieManager::buildVisibleCookies() const
{
    QList<QNetworkCookie> visibleCookies = m_cookies;
    QSet<QString> seenKeys;

    for (const QNetworkCookie &cookie : visibleCookies)
        seenKeys.insert(cookieKey(cookie));

    const QList<QNetworkCookie> diskCookies = loadCookiesFromDisk();
    for (const QNetworkCookie &cookie : diskCookies) {
        const QString key = cookieKey(cookie);
        if (seenKeys.contains(key))
            continue;

        seenKeys.insert(key);
        visibleCookies.append(cookie);
    }

    std::sort(visibleCookies.begin(), visibleCookies.end(), [](const QNetworkCookie &left, const QNetworkCookie &right) {
        const QString leftDomain = left.domain();
        const QString rightDomain = right.domain();
        if (leftDomain != rightDomain)
            return leftDomain < rightDomain;

        const QByteArray leftName = left.name();
        const QByteArray rightName = right.name();
        if (leftName != rightName)
            return leftName < rightName;

        return left.path() < right.path();
    });

    return visibleCookies;
}

QList<QNetworkCookie> CookieManager::loadCookiesFromDisk() const
{
    QList<QNetworkCookie> cookies;
    const QStringList databasePaths = cookieDatabasePaths();
    if (databasePaths.isEmpty())
        return cookies;

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return cookies;

    for (int i = 0; i < databasePaths.size(); ++i) {
        const QString databasePath = databasePaths.at(i);
        const QString tempDatabasePath = QDir(tempDir.path()).filePath(QStringLiteral("Cookies_%1.sqlite").arg(i));
        QFile::remove(tempDatabasePath);
        if (!QFile::copy(databasePath, tempDatabasePath))
            continue;

        const QString walPath = databasePath + QStringLiteral("-wal");
        const QString shmPath = databasePath + QStringLiteral("-shm");
        const QString tempWalPath = tempDatabasePath + QStringLiteral("-wal");
        const QString tempShmPath = tempDatabasePath + QStringLiteral("-shm");
        if (QFile::exists(walPath)) {
            QFile::remove(tempWalPath);
            QFile::copy(walPath, tempWalPath);
        }
        if (QFile::exists(shmPath)) {
            QFile::remove(tempShmPath);
            QFile::copy(shmPath, tempShmPath);
        }

        const QString connectionName = QStringLiteral("ghost-cookie-viewer-%1-%2")
            .arg(i)
            .arg(QRandomGenerator::global()->generate64());

        {
            QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            database.setDatabaseName(tempDatabasePath);
            if (!database.open()) {
                database = QSqlDatabase();
                QSqlDatabase::removeDatabase(connectionName);
                continue;
            }

            QSqlQuery query(database);
            query.setForwardOnly(true);
            if (query.exec(QStringLiteral(
                    "SELECT name, host_key, path, value, is_secure, is_httponly, has_expires, expires_utc "
                    "FROM cookies ORDER BY host_key, name, path"))) {
                while (query.next()) {
                    QNetworkCookie cookie;
                    cookie.setName(query.value(0).toByteArray());
                    cookie.setDomain(query.value(1).toString());
                    cookie.setPath(query.value(2).toString());
                    cookie.setValue(query.value(3).toByteArray());
                    cookie.setSecure(query.value(4).toBool());
                    cookie.setHttpOnly(query.value(5).toBool());

                    const bool hasExpires = query.value(6).toBool();
                    const qint64 expiresUtc = query.value(7).toLongLong();
                    if (hasExpires && expiresUtc > 0)
                        cookie.setExpirationDate(chromiumTimestampToUtc(expiresUtc));

                    cookies.append(cookie);
                }
            }

            database.close();
        }

        QSqlDatabase::removeDatabase(connectionName);
    }

    return cookies;
}

QStringList CookieManager::cookieDatabasePaths() const
{
    if (m_storagePath.isEmpty())
        return {};

    QStringList databasePaths;
    QDirIterator it(m_storagePath,
                    QStringList { QStringLiteral("Cookies") },
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
        databasePaths.append(QDir::toNativeSeparators(it.next()));

    databasePaths.removeDuplicates();
    std::sort(databasePaths.begin(), databasePaths.end());
    return databasePaths;
}

QDateTime CookieManager::chromiumTimestampToUtc(qint64 microsecondsSince1601)
{
    constexpr qint64 kMicrosecondsBetween1601And1970 = 11644473600000000LL;
    const qint64 microsecondsSinceUnixEpoch = microsecondsSince1601 - kMicrosecondsBetween1601And1970;
    const qint64 millisecondsSinceUnixEpoch = microsecondsSinceUnixEpoch / 1000;
    return QDateTime::fromMSecsSinceEpoch(millisecondsSinceUnixEpoch, Qt::UTC);
}

void CookieManager::reload()
{
    if (!m_store)
        return;

    // Do NOT clear m_cookies here. Cookies accumulated during navigation via
    // onCookieAdded() are the ground truth. loadAllCookies() fires cookieAdded
    // for any cookies not yet delivered (e.g. persisted cookies at cold start),
    // and onCookieAdded already deduplicates by name+domain+path.
    // Clearing would wipe the valid in-memory list and rely on loadAllCookies()
    // re-delivering everything, which is unreliable for session cookies.
    m_loading = true;
    m_store->loadAllCookies();

    // Existing cookies can arrive from the browser process after startup with
    // variable latency. Retry for longer before declaring the list empty.
    QTimer::singleShot(kCookieReloadDelayMs, this, [this]() {
        finishReload(kCookieReloadRetries);
    });
}

void CookieManager::onCookieAdded(const QNetworkCookie &cookie)
{
    for (int i = 0; i < m_cookies.size(); ++i) {
        const QNetworkCookie &existing = m_cookies.at(i);
        if (existing.name() == cookie.name()
            && existing.domain() == cookie.domain()
            && existing.path() == cookie.path()) {
            m_cookies.removeAt(i);
            break;
        }
    }

    m_cookies.append(cookie);
    if (!m_loading)
        emit cookiesChanged();
}

void CookieManager::onCookieRemoved(const QNetworkCookie &cookie)
{
    for (int i = 0; i < m_cookies.size(); ++i) {
        const QNetworkCookie &c = m_cookies.at(i);
        if (c.name() == cookie.name()
            && c.domain() == cookie.domain()
            && c.path() == cookie.path()) {
            m_cookies.removeAt(i);
            break;
        }
    }
    emit cookiesChanged();
}

void CookieManager::finishReload(int remainingRetries)
{
    if (!m_store)
        return;

    if (m_cookies.isEmpty() && remainingRetries > 0) {
        m_store->loadAllCookies();
        QTimer::singleShot(kCookieReloadDelayMs, this, [this, remainingRetries]() {
            finishReload(remainingRetries - 1);
        });
        return;
    }

    m_loading = false;
    emit cookiesChanged();
}
