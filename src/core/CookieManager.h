#pragma once

#include <QDateTime>
#include <QList>
#include <QNetworkCookie>
#include <QObject>
#include <QString>

class QWebEngineCookieStore;

class CookieManager : public QObject
{
    Q_OBJECT

public:
    explicit CookieManager(QWebEngineCookieStore *store, QObject *parent = nullptr);

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
    QWebEngineCookieStore *m_store = nullptr;
    QList<QNetworkCookie> m_cookies;
    bool m_loading = false;
};
