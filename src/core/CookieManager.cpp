#include "core/CookieManager.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QWebEngineCookieStore>

CookieManager::CookieManager(QWebEngineCookieStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
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
    QJsonArray arr;
    for (int i = 0; i < m_cookies.size(); ++i) {
        const QNetworkCookie &c = m_cookies.at(i);
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
    if (index < 0 || index >= m_cookies.size() || !m_store)
        return;

    m_store->deleteCookie(m_cookies.at(index));
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

void CookieManager::reload()
{
    if (!m_store)
        return;

    m_loading = true;
    m_cookies.clear();
    m_store->loadAllCookies();

    // Existing cookies can arrive from the browser process after startup with
    // variable latency. Retry a couple of times before declaring the list empty.
    QTimer::singleShot(350, this, [this]() {
        finishReload(2);
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
        QTimer::singleShot(350, this, [this, remainingRetries]() {
            finishReload(remainingRetries - 1);
        });
        return;
    }

    m_loading = false;
    emit cookiesChanged();
}
