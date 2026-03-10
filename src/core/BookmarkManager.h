#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

struct BookmarkEntry
{
    QString id;
    QString title;
    QUrl url;
    QDateTime createdAt;
};

class BookmarkManager : public QObject
{
    Q_OBJECT

public:
    explicit BookmarkManager(QObject *parent = nullptr);
    ~BookmarkManager() override;

    QVector<BookmarkEntry> entries() const { return m_entries; }
    QJsonArray toJsonArray() const;
    bool containsUrl(const QUrl &url) const;
    QString bookmarkIdForUrl(const QUrl &url) const;

    Q_INVOKABLE QString getBookmarksJson() const;
    Q_INVOKABLE bool addBookmark(const QString &title, const QString &url);
    Q_INVOKABLE bool updateBookmark(const QString &id, const QString &title, const QString &url);
    Q_INVOKABLE bool deleteBookmark(const QString &id);
    Q_INVOKABLE QString importBookmarksFromFile();
    Q_INVOKABLE QString exportBookmarksToFile() const;

signals:
    void bookmarksChanged();

private:
    int indexOfId(const QString &id) const;
    int indexOfUrl(const QUrl &url) const;
    bool upsertBookmark(const QString &title, const QUrl &url, bool overwriteTitle, bool *changed = nullptr);
    void load();
    void save() const;
    QString storagePath() const;

    QVector<BookmarkEntry> m_entries;
    mutable bool m_dirty = false;
};