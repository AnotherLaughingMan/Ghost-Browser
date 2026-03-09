#pragma once

#include <QObject>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

class WeatherService : public QObject
{
    Q_OBJECT

public:
    explicit WeatherService(QObject *parent = nullptr);

    Q_INVOKABLE void refresh();

signals:
    void weatherReady(const QString &weatherJson);
    void weatherError(const QString &message);

private:
    void startRequest(int endpointIndex);
    void finishWithError(const QString &message);
    QByteArray extractJsonPayload(const QByteArray &rawBody) const;
    QString normalizeWeatherPayload(const QByteArray &jsonBytes) const;

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QStringList m_endpoints;
    int m_activeEndpointIndex = -1;
};