#include "core/WeatherService.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace {

QString firstValue(const QJsonValue &value, const QString &fallback = QString())
{
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        if (array.isEmpty())
            return fallback;

        const QJsonValue first = array.first();
        if (first.isObject())
            return first.toObject().value(QStringLiteral("value")).toString(fallback).trimmed();

        return first.toVariant().toString().trimmed();
    }

    if (value.isObject())
        return value.toObject().value(QStringLiteral("value")).toString(fallback).trimmed();

    const QString text = value.toVariant().toString().trimmed();
    return text.isEmpty() ? fallback : text;
}

}

WeatherService::WeatherService(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_endpoints {
        QStringLiteral("https://wttr.in/?format=j1"),
        QStringLiteral("https://wttr.in/?format=j1&lang=en")
    }
{
}

void WeatherService::refresh()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    startRequest(0);
}

void WeatherService::startRequest(int endpointIndex)
{
    if (endpointIndex < 0 || endpointIndex >= m_endpoints.size()) {
        finishWithError(QStringLiteral("Weather request failed"));
        return;
    }

    m_activeEndpointIndex = endpointIndex;

    QNetworkRequest request(QUrl(m_endpoints.at(endpointIndex)));
    request.setTransferTimeout(15000);
    request.setRawHeader("Accept", "application/json, text/plain;q=0.9, */*;q=0.8");
    request.setRawHeader("Pragma", "no-cache");
    request.setRawHeader("Cache-Control", "no-cache");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_network->get(request);
    QTimer::singleShot(15000, m_reply, [reply = m_reply]() {
        if (reply && reply->isRunning())
            reply->abort();
    });

    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (!reply)
            return;

        const int endpointIndex = m_activeEndpointIndex;
        const QByteArray body = reply->readAll();
        const QNetworkReply::NetworkError error = reply->error();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        if (error != QNetworkReply::NoError) {
            if (endpointIndex + 1 < m_endpoints.size()) {
                startRequest(endpointIndex + 1);
                return;
            }

            if (error == QNetworkReply::OperationCanceledError) {
                finishWithError(QStringLiteral("Weather request timed out. Try refreshing again."));
                return;
            }

            finishWithError(QStringLiteral("Weather service unavailable%1")
                                .arg(statusCode > 0 ? QStringLiteral(" (%1)").arg(statusCode) : QString()));
            return;
        }

        const QByteArray jsonPayload = extractJsonPayload(body);
        if (jsonPayload.isEmpty()) {
            if (endpointIndex + 1 < m_endpoints.size()) {
                startRequest(endpointIndex + 1);
                return;
            }

            finishWithError(QStringLiteral("Weather service returned an invalid response"));
            return;
        }

        const QString normalized = normalizeWeatherPayload(jsonPayload);
        if (normalized.isEmpty()) {
            if (endpointIndex + 1 < m_endpoints.size()) {
                startRequest(endpointIndex + 1);
                return;
            }

            finishWithError(QStringLiteral("Weather data incomplete"));
            return;
        }

        emit weatherReady(normalized);
    });
}

void WeatherService::finishWithError(const QString &message)
{
    emit weatherError(message);
}

QByteArray WeatherService::extractJsonPayload(const QByteArray &rawBody) const
{
    QJsonParseError error;
    QJsonDocument::fromJson(rawBody, &error);
    if (error.error == QJsonParseError::NoError)
        return rawBody;

    const int start = rawBody.indexOf('{');
    const int end = rawBody.lastIndexOf('}');
    if (start < 0 || end <= start)
        return {};

    const QByteArray sliced = rawBody.mid(start, end - start + 1);
    QJsonDocument::fromJson(sliced, &error);
    return error.error == QJsonParseError::NoError ? sliced : QByteArray();
}

QString WeatherService::normalizeWeatherPayload(const QByteArray &jsonBytes) const
{
    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
    if (!document.isObject())
        return {};

    const QJsonObject root = document.object();
    const QJsonArray currentArray = root.value(QStringLiteral("current_condition")).toArray();
    const QJsonObject current = currentArray.isEmpty() ? QJsonObject() : currentArray.at(0).toObject();
    if (current.isEmpty())
        return {};

    const QJsonArray nearestArray = root.value(QStringLiteral("nearest_area")).toArray();
    const QJsonObject nearest = nearestArray.isEmpty() ? QJsonObject() : nearestArray.at(0).toObject();
    const QString area = firstValue(nearest.value(QStringLiteral("areaName")), QStringLiteral("Your area"));
    const QString region = firstValue(nearest.value(QStringLiteral("region")));
    const QString weatherText = firstValue(current.value(QStringLiteral("weatherDesc")), QStringLiteral("Current conditions"));
    const QString temperature = firstValue(current.value(QStringLiteral("temp_F")), firstValue(current.value(QStringLiteral("temp_C")), QStringLiteral("--")));
    const QString feelsLike = firstValue(current.value(QStringLiteral("FeelsLikeF")), firstValue(current.value(QStringLiteral("FeelsLikeC")), temperature));
    const QString windMiles = firstValue(current.value(QStringLiteral("windspeedMiles")));
    const QString windMetric = firstValue(current.value(QStringLiteral("windspeedKmph")), QStringLiteral("0"));
    const bool hasMiles = !windMiles.isEmpty();

    QJsonObject normalized {
        { QStringLiteral("city"), region.isEmpty() ? area : QStringLiteral("%1, %2").arg(area, region) },
        { QStringLiteral("temperature"), QStringLiteral("%1°").arg(temperature) },
        { QStringLiteral("meta"), QStringLiteral("%1 · Feels like %2° · Wind %3 %4")
            .arg(weatherText,
                 feelsLike,
                 hasMiles ? windMiles : windMetric,
                 hasMiles ? QStringLiteral("mph") : QStringLiteral("km/h")) },
        { QStringLiteral("source"), QStringLiteral("wttr.in") },
        { QStringLiteral("fetchedAt"), QDateTime::currentDateTime().toString(Qt::ISODate) },
    };

    return QString::fromUtf8(QJsonDocument(normalized).toJson(QJsonDocument::Compact));
}