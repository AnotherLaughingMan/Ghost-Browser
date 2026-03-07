#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVector>

struct HistoryEntry
{
    QUrl      url;
    QString   title;
    QDateTime visitedAt;
};

class HistoryManager : public QObject
{
    Q_OBJECT

public:
    explicit HistoryManager(QObject *parent = nullptr);
    ~HistoryManager() override;

    void recordVisit(const QUrl &url, const QString &title);
    void clear();

    QVector<HistoryEntry> entries() const { return m_entries; }
    QJsonArray toJsonArray() const;

    Q_INVOKABLE QString getHistoryJson() const;
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE void clearByAge(const QString &range);
    Q_INVOKABLE void deleteEntry(int index);

signals:
    void historyChanged();

private:
    void load();
    void save() const;
    QString storagePath() const;

    QVector<HistoryEntry> m_entries;
    QSet<quint32>         m_urlHashes;   // CRC32C of each stored URL for O(1) dedup
    mutable bool m_dirty = false;
};
