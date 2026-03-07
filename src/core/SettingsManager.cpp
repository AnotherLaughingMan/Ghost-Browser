#include "core/SettingsManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
{
    load();
}

QString SettingsManager::startupBehavior() const
{
    return value(QStringLiteral("general.startupBehavior")).toString();
}

QString SettingsManager::searchEngine() const
{
    const QString engine = value(QStringLiteral("general.searchEngine")).toString();
    return engine.isEmpty() ? QStringLiteral("duckduckgo") : engine;
}

QUrl SettingsManager::homePageUrl() const
{
    const QString homePage = value(QStringLiteral("general.homePage")).toString();
    return homePage.isEmpty() ? QUrl(QStringLiteral("ghost://newtab")) : QUrl(homePage);
}

QVariant SettingsManager::value(const QString &path) const
{
    return valueAtPath(path);
}

QString SettingsManager::getSettingsJson() const
{
    return QString::fromUtf8(QJsonDocument(m_settings).toJson(QJsonDocument::Compact));
}

bool SettingsManager::updateSetting(const QString &path, const QVariant &value)
{
    if (!setValueAtPath(path, QJsonValue::fromVariant(value)))
        return false;

    if (!writeJsonFile(userSettingsPath(), m_settings))
        return false;

    emit settingsChanged(getSettingsJson());
    return true;
}

QString SettingsManager::chooseDownloadPath()
{
    QString currentPath = value(QStringLiteral("downloads.defaultPath")).toString();
    if (currentPath.isEmpty()) {
        currentPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (currentPath.isEmpty())
            currentPath = QDir::homePath();
    }

    const QString selectedPath = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("Choose Download Folder"),
        currentPath);

    if (selectedPath.isEmpty())
        return {};

    if (!updateSetting(QStringLiteral("downloads.defaultPath"), selectedPath))
        return {};

    return selectedPath;
}

bool SettingsManager::resetToDefaults()
{
    m_settings = builtInDefaults();
    if (!writeJsonFile(userSettingsPath(), m_settings))
        return false;

    emit settingsChanged(getSettingsJson());
    return true;
}

void SettingsManager::requestClearBrowsingData()
{
    emit clearBrowsingDataRequested();
}

void SettingsManager::load()
{
    QJsonObject defaults = readJsonFile(defaultSettingsPath());
    if (defaults.isEmpty())
        defaults = builtInDefaults();

    const QJsonObject userSettings = readJsonFile(userSettingsPath());
    m_settings = mergeObjects(defaults, userSettings);

    if (!userSettings.contains(QStringLiteral("version")))
        writeJsonFile(userSettingsPath(), m_settings);
}

QJsonObject SettingsManager::builtInDefaults() const
{
    return QJsonObject {
        { QStringLiteral("version"), 1 },
        { QStringLiteral("general"), QJsonObject {
            { QStringLiteral("startupBehavior"), QStringLiteral("newTab") },
            { QStringLiteral("homePage"), QStringLiteral("ghost://newtab") },
            { QStringLiteral("searchEngine"), QStringLiteral("duckduckgo") },
        } },
        { QStringLiteral("appearance"), QJsonObject {
            { QStringLiteral("theme"), QStringLiteral("dark") },
            { QStringLiteral("showBookmarksBar"), false },
            { QStringLiteral("fontSize"), 16 },
            { QStringLiteral("zoomLevel"), 100 },
        } },
        { QStringLiteral("content"), QJsonObject {
            { QStringLiteral("autoplay"), true },
            { QStringLiteral("fullScreenVideo"), true },
        } },
        { QStringLiteral("privacy"), QJsonObject {
            { QStringLiteral("doNotTrack"), true },
            { QStringLiteral("blockThirdPartyCookies"), true },
            { QStringLiteral("clearDataOnExit"), false },
            { QStringLiteral("httpsOnly"), false },
        } },
        { QStringLiteral("downloads"), QJsonObject {
            { QStringLiteral("askWhereToSave"), false },
            { QStringLiteral("defaultPath"), QString() },
        } },
        { QStringLiteral("languages"), QJsonObject {
            { QStringLiteral("spellCheck"), true },
        } },
        { QStringLiteral("system"), QJsonObject {
            { QStringLiteral("hardwareAcceleration"), true },
            { QStringLiteral("backgroundApps"), false },
            { QStringLiteral("proxyMode"), QStringLiteral("system") },
        } },
        { QStringLiteral("protection"), QJsonObject {
            { QStringLiteral("trackingLevel"), QStringLiteral("aggressive") },
            { QStringLiteral("httpsUpgrade"), true },
            { QStringLiteral("blockFingerprinting"), true },
            { QStringLiteral("blockScripts"), false },
            { QStringLiteral("safeBrowsing"), true },
        } },
        { QStringLiteral("accessibility"), QJsonObject {
            { QStringLiteral("caretBrowsing"), false },
            { QStringLiteral("highContrast"), false },
        } },
    };
}

QString SettingsManager::defaultSettingsPath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/defaults.json"));
}

QString SettingsManager::userSettingsPath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty())
        configDir = QCoreApplication::applicationDirPath();

    QDir dir(configDir);
    dir.mkpath(QStringLiteral("."));
    return dir.filePath(QStringLiteral("settings.json"));
}

QJsonObject SettingsManager::readJsonFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject {};
}

bool SettingsManager::writeJsonFile(const QString &path, const QJsonObject &object) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

QVariant SettingsManager::valueAtPath(const QString &path) const
{
    QStringList parts = path.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return {};

    QJsonValue current = m_settings;
    for (const QString &part : parts) {
        if (!current.isObject())
            return {};
        current = current.toObject().value(part);
    }

    return current.toVariant();
}

bool SettingsManager::setValueAtPath(const QString &path, const QJsonValue &value)
{
    QStringList parts = path.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;

    const auto setRecursive = [&](auto &&self, QJsonObject &object, int index) -> void {
        const QString &part = parts.at(index);
        if (index == parts.size() - 1) {
            object.insert(part, value);
            return;
        }

        QJsonObject child = object.value(part).toObject();
        self(self, child, index + 1);
        object.insert(part, child);
    };

    setRecursive(setRecursive, m_settings, 0);
    return true;
}

QJsonObject SettingsManager::mergeObjects(const QJsonObject &base, const QJsonObject &overlay)
{
    QJsonObject merged = base;
    for (auto it = overlay.begin(); it != overlay.end(); ++it) {
        if (it->isObject() && merged.value(it.key()).isObject())
            merged.insert(it.key(), mergeObjects(merged.value(it.key()).toObject(), it->toObject()));
        else
            merged.insert(it.key(), *it);
    }
    return merged;
}