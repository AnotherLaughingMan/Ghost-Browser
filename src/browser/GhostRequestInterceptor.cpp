#include "browser/GhostRequestInterceptor.h"

#include "core/ProtectionDiagnostics.h"
#include "core/SettingsManager.h"

#include <QStringList>
#include <QUrl>
#include <QtWebEngineCore/QWebEngineUrlRequestInfo>

GhostRequestInterceptor::GhostRequestInterceptor(SettingsManager *settings,
                                                 ProtectionDiagnostics *diagnostics,
                                                 QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
    , m_settings(settings)
    , m_diagnostics(diagnostics)
{
    refreshSettings();
}

void GhostRequestInterceptor::refreshSettings()
{
    if (!m_settings)
        return;
    m_dntEnabled = m_settings->value(QStringLiteral("privacy.doNotTrack")).toBool();
    m_httpsOnly  = m_settings->value(QStringLiteral("privacy.httpsOnly")).toBool();
    m_blockFingerprinting = m_settings->value(QStringLiteral("protection.blockFingerprinting")).toBool();
    m_httpsUpgrade = m_settings->value(QStringLiteral("protection.httpsUpgrade")).toBool();
    m_blockScripts = m_settings->value(QStringLiteral("protection.blockScripts")).toBool();
    m_safeBrowsing = m_settings->value(QStringLiteral("protection.safeBrowsing")).toBool();

    const QString trackingLevel = m_settings->value(QStringLiteral("protection.trackingLevel")).toString().trimmed().toLower();
    if (trackingLevel == QLatin1String("aggressive"))
        m_trackingLevel = TrackingLevel::Aggressive;
    else if (trackingLevel == QLatin1String("standard"))
        m_trackingLevel = TrackingLevel::Standard;
    else
        m_trackingLevel = TrackingLevel::Disabled;
}

void GhostRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (m_dntEnabled)
        info.setHttpHeader("DNT", "1");

    if (m_blockFingerprinting) {
        info.setHttpHeader("Sec-CH-UA", "\"Ghost\";v=\"1\", \"Not A(Brand\";v=\"99\"");
        info.setHttpHeader("Sec-CH-UA-Mobile", "?0");
        info.setHttpHeader("Sec-CH-UA-Platform", "\"Windows\"");
        info.setHttpHeader("Sec-CH-UA-Platform-Version", "\"15.0.0\"");
        info.setHttpHeader("Sec-CH-UA-Arch", "\"x86\"");
        info.setHttpHeader("Sec-CH-UA-Bitness", "\"64\"");
    }

    QString blockCategory;
    QString blockDetail;
    if (shouldBlockRequest(info, &blockCategory, &blockDetail)) {
        if (m_diagnostics) {
            const QUrl contextUrl = info.initiator().isValid() ? info.initiator() : info.firstPartyUrl();
            m_diagnostics->recordEvent(QStringLiteral("blocked"),
                                       blockCategory,
                                       info.requestUrl(),
                                       contextUrl,
                                       blockDetail);
        }
        info.block(true);
        return;
    }

    QString upgradeDetail;
    if (shouldUpgradeRequest(info, &upgradeDetail)) {
        if (m_diagnostics) {
            const QUrl contextUrl = info.initiator().isValid() ? info.initiator() : info.firstPartyUrl();
            m_diagnostics->recordEvent(QStringLiteral("upgraded"),
                                       QStringLiteral("https"),
                                       info.requestUrl(),
                                       contextUrl,
                                       upgradeDetail);
        }
        QUrl upgraded = info.requestUrl();
        upgraded.setScheme(QStringLiteral("https"));
        info.redirect(upgraded);
    }
}

bool GhostRequestInterceptor::isLocalHost(const QUrl &url)
{
    const QString host = url.host().toLower();
    return host == QLatin1String("localhost")
        || host == QLatin1String("127.0.0.1")
        || host == QLatin1String("::1");
}

QString GhostRequestInterceptor::registrableDomain(const QString &host)
{
    const QString normalized = host.trimmed().toLower();
    const QStringList parts = normalized.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.size() <= 2)
        return normalized;

    return parts.at(parts.size() - 2) + QLatin1Char('.') + parts.at(parts.size() - 1);
}

bool GhostRequestInterceptor::isThirdPartyRequest(const QUrl &requestUrl, const QUrl &firstPartyUrl, const QUrl &initiator)
{
    const QString requestHost = requestUrl.host().trimmed().toLower();
    const QString contextHost = initiator.host().isEmpty()
        ? firstPartyUrl.host().trimmed().toLower()
        : initiator.host().trimmed().toLower();

    if (requestHost.isEmpty() || contextHost.isEmpty())
        return false;

    return registrableDomain(requestHost) != registrableDomain(contextHost);
}

bool GhostRequestInterceptor::hostMatchesList(const QString &host, const QStringList &patterns)
{
    for (const QString &pattern : patterns) {
        if (host == pattern || host.endsWith(QLatin1Char('.') + pattern))
            return true;
    }
    return false;
}

bool GhostRequestInterceptor::hasBlockedDownloadSuffix(const QString &path)
{
    const QString normalized = path.trimmed().toLower();
    static const QStringList blockedSuffixes = {
        QStringLiteral(".bat"),
        QStringLiteral(".cmd"),
        QStringLiteral(".com"),
        QStringLiteral(".exe"),
        QStringLiteral(".js"),
        QStringLiteral(".jse"),
        QStringLiteral(".lnk"),
        QStringLiteral(".msi"),
        QStringLiteral(".ps1"),
        QStringLiteral(".reg"),
        QStringLiteral(".scr"),
        QStringLiteral(".vbs")
    };

    for (const QString &suffix : blockedSuffixes) {
        if (normalized.endsWith(suffix))
            return true;
    }

    return false;
}

bool GhostRequestInterceptor::shouldUpgradeRequest(const QWebEngineUrlRequestInfo &info, QString *detail) const
{
    const QUrl requestUrl = info.requestUrl();
    if (requestUrl.scheme() != QLatin1String("http") || isLocalHost(requestUrl))
        return false;

    if (m_httpsOnly) {
        if (detail)
            *detail = QStringLiteral("HTTPS-Only mode redirected an insecure request.");
        return true;
    }

    if (!m_httpsUpgrade)
        return false;

    switch (info.resourceType()) {
    case QWebEngineUrlRequestInfo::ResourceTypeMainFrame:
    case QWebEngineUrlRequestInfo::ResourceTypeSubFrame:
    case QWebEngineUrlRequestInfo::ResourceTypeNavigationPreloadMainFrame:
    case QWebEngineUrlRequestInfo::ResourceTypeNavigationPreloadSubFrame:
        if (detail)
            *detail = QStringLiteral("Protection upgraded a navigation request to HTTPS.");
        return true;
    default:
        return false;
    }
}

bool GhostRequestInterceptor::shouldBlockRequest(const QWebEngineUrlRequestInfo &info,
                                                 QString *category,
                                                 QString *detail) const
{
    const QUrl requestUrl = info.requestUrl();
    if (!requestUrl.isValid() || isLocalHost(requestUrl))
        return false;

    const bool thirdParty = isThirdPartyRequest(requestUrl, info.firstPartyUrl(), info.initiator());
    const QString host = requestUrl.host().trimmed().toLower();

    if (m_safeBrowsing) {
        static const QStringList blockedHosts = {
            QStringLiteral("malware.testcategory.com"),
            QStringLiteral("phishing.testcategory.com"),
            QStringLiteral("testsafebrowsing.appspot.com"),
            QStringLiteral("malware.wicar.org"),
            QStringLiteral("phishing.wicar.org"),
            QStringLiteral("drive-by-download.wicar.org"),
            QStringLiteral("download-malware.wicar.org")
        };

        if (hostMatchesList(host, blockedHosts)) {
            if (category)
                *category = QStringLiteral("safe-browsing");
            if (detail)
                *detail = QStringLiteral("Blocked a request to a known malicious or phishing test host.");
            return true;
        }

        if (info.isDownload() && hasBlockedDownloadSuffix(requestUrl.path())) {
            if (category)
                *category = QStringLiteral("unsafe-download");
            if (detail)
                *detail = QStringLiteral("Blocked a risky download type while Safe Browsing was enabled.");
            return true;
        }
    }

    if (m_blockScripts
        && thirdParty
        && info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeScript) {
        if (category)
            *category = QStringLiteral("third-party-script");
        if (detail)
            *detail = QStringLiteral("Blocked a third-party script request.");
        return true;
    }

    if (m_trackingLevel == TrackingLevel::Disabled)
        return false;

    static const QStringList trackerHosts = {
        QStringLiteral("google-analytics.com"),
        QStringLiteral("googletagmanager.com"),
        QStringLiteral("doubleclick.net"),
        QStringLiteral("googlesyndication.com"),
        QStringLiteral("googleadservices.com"),
        QStringLiteral("facebook.net"),
        QStringLiteral("connect.facebook.net"),
        QStringLiteral("scorecardresearch.com"),
        QStringLiteral("hotjar.com"),
        QStringLiteral("segment.com"),
        QStringLiteral("mixpanel.com"),
        QStringLiteral("amplitude.com"),
        QStringLiteral("branch.io"),
        QStringLiteral("taboola.com"),
        QStringLiteral("outbrain.com")
    };

    static const QStringList adHosts = {
        QStringLiteral("adsrvr.org"),
        QStringLiteral("adnxs.com"),
        QStringLiteral("criteo.com"),
        QStringLiteral("criteo.net"),
        QStringLiteral("pubmatic.com"),
        QStringLiteral("rubiconproject.com"),
        QStringLiteral("openx.net"),
        QStringLiteral("advertising.com"),
        QStringLiteral("zedo.com")
    };

    const bool matchedTracker = hostMatchesList(host, trackerHosts) || hostMatchesList(host, adHosts);
    if (!matchedTracker)
        return false;

    if (!thirdParty && m_trackingLevel != TrackingLevel::Aggressive)
        return false;

    switch (m_trackingLevel) {
    case TrackingLevel::Standard:
        switch (info.resourceType()) {
        case QWebEngineUrlRequestInfo::ResourceTypePing:
        case QWebEngineUrlRequestInfo::ResourceTypeScript:
        case QWebEngineUrlRequestInfo::ResourceTypeXhr:
        case QWebEngineUrlRequestInfo::ResourceTypeSubResource:
        case QWebEngineUrlRequestInfo::ResourceTypeImage:
            if (category)
                *category = QStringLiteral("tracker");
            if (detail)
                *detail = QStringLiteral("Blocked a tracker or ad request under Standard protection.");
            return true;
        default:
            return false;
        }
    case TrackingLevel::Aggressive:
        switch (info.resourceType()) {
        case QWebEngineUrlRequestInfo::ResourceTypeMainFrame:
        case QWebEngineUrlRequestInfo::ResourceTypeSubFrame:
            return false;
        default:
            if (category)
                *category = QStringLiteral("tracker");
            if (detail)
                *detail = QStringLiteral("Blocked a tracker or ad request under Aggressive protection.");
            return true;
        }
    case TrackingLevel::Disabled:
        return false;
    }

    return false;
}