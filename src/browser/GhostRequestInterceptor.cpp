#include "browser/GhostRequestInterceptor.h"

#include "core/SettingsManager.h"

#include <QUrl>
#include <QtWebEngineCore/QWebEngineUrlRequestInfo>

GhostRequestInterceptor::GhostRequestInterceptor(SettingsManager *settings, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
    , m_settings(settings)
{
    refreshSettings();
}

void GhostRequestInterceptor::refreshSettings()
{
    if (!m_settings)
        return;
    m_dntEnabled = m_settings->value(QStringLiteral("privacy.doNotTrack")).toBool();
    m_httpsOnly  = m_settings->value(QStringLiteral("privacy.httpsOnly")).toBool();
}

void GhostRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (m_dntEnabled)
        info.setHttpHeader("DNT", "1");

    if (m_httpsOnly) {
        const QUrl requestUrl = info.requestUrl();
        if (requestUrl.scheme() == QLatin1String("http")
            && !isLocalHost(requestUrl)) {
            QUrl upgraded = requestUrl;
            upgraded.setScheme(QStringLiteral("https"));
            info.redirect(upgraded);
        }
    }
}

bool GhostRequestInterceptor::isLocalHost(const QUrl &url)
{
    const QString host = url.host().toLower();
    return host == QLatin1String("localhost")
        || host == QLatin1String("127.0.0.1")
        || host == QLatin1String("::1");
}