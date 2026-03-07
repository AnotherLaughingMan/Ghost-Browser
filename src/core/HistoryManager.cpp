#include "core/HistoryManager.h"
#include "core/asm_fallbacks.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

static constexpr int MaxHistoryEntries = 10000;

HistoryManager::HistoryManager(QObject *parent)
    : QObject(parent)
{
    load();
}

HistoryManager::~HistoryManager()
{
    if (m_dirty)
        save();
}

void HistoryManager::recordVisit(const QUrl &url, const QString &title)
{
    if (!url.isValid() || url.scheme() == QLatin1String("qrc"))
        return;

    // CRC32C dedup: skip if this exact URL was visited within the last 60 seconds.
    // Uses hardware CRC32 instruction (SSE4.2) on x64 Windows, software table elsewhere.
    const QByteArray urlBytes = url.toString().toUtf8();
    const quint32 urlHash = ghost::crc32c(urlBytes.constData(), static_cast<size_t>(urlBytes.size()));

    if (m_urlHashes.contains(urlHash)) {
        // Check if the most recent matching entry is within 60 s to suppress rapid duplicates.
        for (const HistoryEntry &e : std::as_const(m_entries)) {
            const QByteArray eb = e.url.toString().toUtf8();
            if (ghost::crc32c(eb.constData(), static_cast<size_t>(eb.size())) == urlHash) {
                const qint64 ageSecs = e.visitedAt.secsTo(QDateTime::currentDateTimeUtc());
                if (ageSecs >= 0 && ageSecs < 60)
                    return;
                break;
            }
        }
    }
    m_urlHashes.insert(urlHash);

    HistoryEntry entry;
    entry.url       = url;
    entry.title     = title.isEmpty() ? url.toString() : title;
    entry.visitedAt = QDateTime::currentDateTimeUtc();

    m_entries.prepend(entry);

    if (m_entries.size() > MaxHistoryEntries)
        m_entries.resize(MaxHistoryEntries);

    m_dirty = true;
    save();
    emit historyChanged();
}

void HistoryManager::clear()
{
    m_entries.clear();
    m_urlHashes.clear();
    m_dirty = true;
    save();
    emit historyChanged();
}

QString HistoryManager::getHistoryJson() const
{
    return QString::fromUtf8(QJsonDocument(toJsonArray()).toJson(QJsonDocument::Compact));
}

void HistoryManager::clearAll()
{
    clear();
}

void HistoryManager::clearByAge(const QString &range)
{
    if (range == QLatin1String("forever")) {
        clear();
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

    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
                        [&cutoff](const HistoryEntry &e) { return e.visitedAt >= cutoff; }),
        m_entries.end());

    m_dirty = true;
    save();
    emit historyChanged();
}

void HistoryManager::deleteEntry(int index)
{
    if (index < 0 || index >= m_entries.size())
        return;

    m_entries.removeAt(index);
    m_dirty = true;
    save();
    emit historyChanged();
}

QJsonArray HistoryManager::toJsonArray() const
{
    QJsonArray arr;
    for (const auto &e : m_entries) {
        arr.append(QJsonObject {
            { QStringLiteral("url"),   e.url.toString() },
            { QStringLiteral("title"), e.title },
            { QStringLiteral("time"),  e.visitedAt.toString(Qt::ISODate) },
        });
    }
    return arr;
}

void HistoryManager::load()
{
    QFile file(storagePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QByteArray raw = file.readAll();

    // Warm CPU cache lines before the JSON parser makes its first pass.
    // PREFETCHT0 every 64 bytes so the data is in L1 when Qt reads it.
    ghost::prefetch_range(raw.constData(), static_cast<size_t>(raw.size()));

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray())
        return;

    const QJsonArray arr = doc.array();
    m_entries.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        const QJsonObject obj = val.toObject();
        HistoryEntry entry;
        entry.url       = QUrl(obj.value(QStringLiteral("url")).toString());
        entry.title     = obj.value(QStringLiteral("title")).toString();
        entry.visitedAt = QDateTime::fromString(obj.value(QStringLiteral("time")).toString(), Qt::ISODate);
        if (entry.url.isValid()) {
            m_entries.append(entry);
            const QByteArray b = entry.url.toString().toUtf8();
            m_urlHashes.insert(ghost::crc32c(b.constData(), static_cast<size_t>(b.size())));
        }
    }
}

void HistoryManager::save() const
{
    const QString path = storagePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(QJsonDocument(toJsonArray()).toJson(QJsonDocument::Compact));
    m_dirty = false;
}

QString HistoryManager::storagePath() const
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(configDir).filePath(QStringLiteral("history.json"));
}
