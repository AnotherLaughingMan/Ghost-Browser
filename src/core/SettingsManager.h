#pragma once

#include <QJsonObject>
#include <QObject>
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
    Q_INVOKABLE QString chooseDownloadPath();
    Q_INVOKABLE bool resetToDefaults();
    Q_INVOKABLE void requestClearBrowsingData();

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
    static QJsonObject mergeObjects(const QJsonObject &base, const QJsonObject &overlay);

    QJsonObject m_settings;
};