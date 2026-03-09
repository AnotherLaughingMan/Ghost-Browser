#pragma once

#include <QDateTime>
#include <QList>
#include <QNetworkCookie>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>

class QWebEngineCookieStore;

class CookieManager : public QObject
{
    Q_OBJECT

public:
    explicit CookieManager(QWebEngineCookieStore *store, const QString &storagePath, QObject *parent = nullptr);
    void setBlockThirdPartyCookies(bool enabled);

    Q_INVOKABLE QString getCookiesJson() const;
    Q_INVOKABLE void deleteByIndex(int index);
    Q_INVOKABLE void clearByAge(const QString &range);
    Q_INVOKABLE void clearAll();
    Q_INVOKABLE void reload();

signals:
    void cookiesChanged();

private slots:
    void onCookieAdded(const QNetworkCookie &cookie);
    void onCookieRemoved(const QNetworkCookie &cookie);

private:
    QList<QNetworkCookie> buildVisibleCookies() const;
    QList<QNetworkCookie> loadCookiesFromDisk() const;
    QStringList cookieDatabasePaths() const;
    static QDateTime chromiumTimestampToUtc(qint64 microsecondsSince1601);
    void finishReload(int remainingRetries);

    QWebEngineCookieStore *m_store = nullptr;
    QString m_storagePath;
    QList<QNetworkCookie> m_cookies;
    mutable QList<QNetworkCookie> m_visibleCookies;
    bool m_loading = false;
    std::atomic_bool m_blockThirdPartyCookies = false;
};
