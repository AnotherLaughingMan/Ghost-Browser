#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariant>

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    explicit SettingsManager(QObject *parent = nullptr);

    QString startupBehavior() const;
    QString searchEngine() const;
    QUrl homePageUrl() const;
    QVariant value(const QString &path) const;

    Q_INVOKABLE QString getSettingsJson() const;
    Q_INVOKABLE bool updateSetting(const QString &path, const QVariant &value);
    Q_INVOKABLE QString importSettingsFromFile();
    Q_INVOKABLE QString chooseDownloadPath();
    Q_INVOKABLE bool openDefaultAppsSettings();
    Q_INVOKABLE QString getDefaultBrowserStatus() const;
    Q_INVOKABLE bool resetToDefaults();
    Q_INVOKABLE void requestClearBrowsingData();
    Q_INVOKABLE QString getSitePermissionRulesJson() const;
    Q_INVOKABLE bool upsertSitePermissionRule(const QString &permissionType, const QString &origin, const QString &policy);
    Q_INVOKABLE bool removeSitePermissionRule(const QString &permissionType, const QString &origin);

    QString sitePermissionRule(const QString &permissionType, const QUrl &origin) const;
    bool rememberSitePermissionRequest(const QString &permissionType, const QUrl &origin);

signals:
    void settingsChanged(const QString &json);
    void clearBrowsingDataRequested();

private:
    void load();
    QJsonObject builtInDefaults() const;
    QString defaultSettingsPath() const;
    QString userSettingsPath() const;
    QJsonObject readJsonFile(const QString &path) const;
    bool writeJsonFile(const QString &path, const QJsonObject &object) const;
    QVariant valueAtPath(const QString &path) const;
    bool setValueAtPath(const QString &path, const QJsonValue &value);
    bool writeSettings();
    static QString normalizeOrigin(const QUrl &url);
    static bool isSupportedPermissionType(const QString &permissionType);
    static QJsonObject mergeObjects(const QJsonObject &base, const QJsonObject &overlay);

    QJsonObject m_settings;
};