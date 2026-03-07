#pragma once

#include <QObject>
#include <QPointer>
#include <QtWebEngineCore/QWebEngineUrlRequestInterceptor>

class SettingsManager;
class QWebEngineUrlRequestInfo;

class GhostRequestInterceptor final : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    explicit GhostRequestInterceptor(SettingsManager *settings, QObject *parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

public slots:
    void refreshSettings();

private:
    static bool isLocalHost(const QUrl &url);

    QPointer<SettingsManager> m_settings;
    bool m_dntEnabled   = false;
    bool m_httpsOnly    = false;
};