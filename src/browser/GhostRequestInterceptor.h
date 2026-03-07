#pragma once

#include <QObject>
#include <QPointer>
#include <QtWebEngineCore/QWebEngineUrlRequestInterceptor>

class SettingsManager;
class QWebEngineUrlRequestInfo;

class GhostRequestInterceptor final : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

    enum class TrackingLevel {
        Disabled,
        Standard,
        Aggressive,
    };

public:
    explicit GhostRequestInterceptor(SettingsManager *settings, QObject *parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

public slots:
    void refreshSettings();

private:
    static bool isLocalHost(const QUrl &url);
    static QString registrableDomain(const QString &host);
    static bool isThirdPartyRequest(const QUrl &requestUrl, const QUrl &firstPartyUrl, const QUrl &initiator);
    static bool hostMatchesList(const QString &host, const QStringList &patterns);
    static bool hasBlockedDownloadSuffix(const QString &path);
    bool shouldUpgradeRequest(const QWebEngineUrlRequestInfo &info) const;
    bool shouldBlockRequest(const QWebEngineUrlRequestInfo &info) const;

    QPointer<SettingsManager> m_settings;
    bool m_dntEnabled   = false;
    bool m_httpsOnly    = false;
    bool m_httpsUpgrade = false;
    bool m_blockScripts = false;
    bool m_safeBrowsing = false;
    TrackingLevel m_trackingLevel = TrackingLevel::Disabled;
};