#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QtWebEngineCore/QWebEngineUrlRequestInterceptor>

class ProtectionDiagnostics;
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
    explicit GhostRequestInterceptor(SettingsManager *settings,
                                     ProtectionDiagnostics *diagnostics,
                                     QObject *parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

public slots:
    void refreshSettings();

private:
    static bool isLocalHost(const QUrl &url);
    static QString registrableDomain(const QString &host);
    static bool isThirdPartyRequest(const QUrl &requestUrl, const QUrl &firstPartyUrl, const QUrl &initiator);
    static bool hostMatchesList(const QString &host, const QStringList &patterns);
    static bool hasBlockedDownloadSuffix(const QString &path);
    bool shouldUpgradeRequest(const QWebEngineUrlRequestInfo &info, QString *detail = nullptr) const;
    bool shouldBlockRequest(const QWebEngineUrlRequestInfo &info,
                            QString *category = nullptr,
                            QString *detail = nullptr) const;

    QPointer<SettingsManager> m_settings;
    QPointer<ProtectionDiagnostics> m_diagnostics;
    bool m_dntEnabled   = false;
    bool m_httpsOnly    = false;
    bool m_blockFingerprinting = false;
    bool m_httpsUpgrade = false;
    bool m_blockScripts = false;
    bool m_safeBrowsing = false;
    TrackingLevel m_trackingLevel = TrackingLevel::Disabled;
};