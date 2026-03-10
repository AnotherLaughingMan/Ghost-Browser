#include "core/SettingsManager.h"

#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMessageBox>
#include <QStandardPaths>

#include <QDebug>
#include <QDesktopServices>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

QWidget *activeDialogParent()
{
    return QApplication::activeWindow();
}

#ifdef Q_OS_WIN
QString readRegistryString(HKEY root, const QString &subKey, const wchar_t *valueName)
{
    DWORD type = 0;
    DWORD size = 0;
    const std::wstring nativeSubKey = subKey.toStdWString();
    if (RegGetValueW(root, nativeSubKey.c_str(), valueName, RRF_RT_REG_SZ, &type, nullptr, &size) != ERROR_SUCCESS || size == 0)
        return {};

    std::wstring buffer(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, nativeSubKey.c_str(), valueName, RRF_RT_REG_SZ, &type, buffer.data(), &size) != ERROR_SUCCESS)
        return {};

    const size_t terminator = buffer.find(L'\0');
    if (terminator != std::wstring::npos)
        buffer.resize(terminator);
    return QString::fromStdWString(buffer);
}

QString executablePathFromCommand(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty())
        return {};

    QString executable;
    if (trimmed.startsWith(QLatin1Char('"'))) {
        const int endQuote = trimmed.indexOf(QLatin1Char('"'), 1);
        executable = endQuote > 1 ? trimmed.mid(1, endQuote - 1) : trimmed.mid(1);
    } else {
        const int firstSpace = trimmed.indexOf(QLatin1Char(' '));
        executable = firstSpace > 0 ? trimmed.left(firstSpace) : trimmed;
    }

    const QFileInfo fileInfo(executable);
    const QString canonical = fileInfo.canonicalFilePath();
    return (canonical.isEmpty() ? fileInfo.absoluteFilePath() : canonical).toLower();
}

QString commandForProgId(const QString &progId)
{
    if (progId.isEmpty())
        return {};

    const QString classesPath = progId + QStringLiteral("\\shell\\open\\command");
    QString command = readRegistryString(HKEY_CURRENT_USER, QStringLiteral("Software\\Classes\\") + classesPath, nullptr);
    if (!command.isEmpty())
        return command;

    command = readRegistryString(HKEY_LOCAL_MACHINE, QStringLiteral("Software\\Classes\\") + classesPath, nullptr);
    if (!command.isEmpty())
        return command;

    return readRegistryString(HKEY_CLASSES_ROOT, classesPath, nullptr);
}
#endif

}

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
    qDebug() << "[SettingsManager] updateSetting" << path << "=" << value
             << "variant-type:" << value.typeName();
    const QJsonValue jv = QJsonValue::fromVariant(value);
    qDebug() << "[SettingsManager] QJsonValue type:" << jv.type() << "toVariant:" << jv.toVariant();
    if (!setValueAtPath(path, jv))
        return false;

    if (!writeSettings()) {
        qDebug() << "[SettingsManager] writeSettings FAILED for" << path;
        return false;
    }

    qDebug() << "[SettingsManager] written OK for" << path
             << "verify:" << valueAtPath(path);
    return true;
}

QString SettingsManager::importSettingsFromFile()
{
    QString startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (startDir.isEmpty())
        startDir = QDir::homePath();

    const QString selectedFile = QFileDialog::getOpenFileName(
        activeDialogParent(),
        QStringLiteral("Import Settings"),
        startDir,
        QStringLiteral("JSON Files (*.json);;All Files (*.*)"));

    if (selectedFile.isEmpty())
        return {};

    const QJsonObject importedSettings = readJsonFile(selectedFile);
    if (importedSettings.isEmpty()) {
        QMessageBox::warning(activeDialogParent(),
                             QStringLiteral("Import Failed"),
                             QStringLiteral("The selected file did not contain a valid settings object."));
        return {};
    }

    QJsonObject defaults = readJsonFile(defaultSettingsPath());
    if (defaults.isEmpty())
        defaults = builtInDefaults();

    m_settings = mergeObjects(defaults, importedSettings);
    if (!writeSettings()) {
        QMessageBox::warning(activeDialogParent(),
                             QStringLiteral("Import Failed"),
                             QStringLiteral("Ghost could not write the imported settings to disk."));
        return {};
    }

    return selectedFile;
}

QString SettingsManager::getSitePermissionRulesJson() const
{
    const QJsonValue rules = m_settings.value(QStringLiteral("content"))
        .toObject()
        .value(QStringLiteral("sitePermissionRules"));
    return QString::fromUtf8(QJsonDocument(rules.toObject()).toJson(QJsonDocument::Compact));
}

bool SettingsManager::upsertSitePermissionRule(const QString &permissionType, const QString &origin, const QString &policy)
{
    const QString normalizedPermissionType = permissionType.trimmed().toLower();
    const QString normalizedOrigin = normalizeOrigin(QUrl(origin.trimmed()));
    const QString normalizedPolicy = policy.trimmed().toLower();
    if (!isSupportedPermissionType(normalizedPermissionType)
        || normalizedOrigin.isEmpty()
        || (normalizedPolicy != QLatin1String("ask")
            && normalizedPolicy != QLatin1String("allow")
            && normalizedPolicy != QLatin1String("block"))) {
        return false;
    }

    QJsonObject content = m_settings.value(QStringLiteral("content")).toObject();
    QJsonObject rules = content.value(QStringLiteral("sitePermissionRules")).toObject();
    QJsonArray entries = rules.value(normalizedPermissionType).toArray();

    bool updated = false;
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject entry = entries.at(i).toObject();
        if (entry.value(QStringLiteral("origin")).toString() != normalizedOrigin)
            continue;

        entry.insert(QStringLiteral("policy"), normalizedPolicy);
        entries.replace(i, entry);
        updated = true;
        break;
    }

    if (!updated) {
        entries.append(QJsonObject {
            { QStringLiteral("origin"), normalizedOrigin },
            { QStringLiteral("policy"), normalizedPolicy },
        });
    }

    rules.insert(normalizedPermissionType, entries);
    content.insert(QStringLiteral("sitePermissionRules"), rules);
    m_settings.insert(QStringLiteral("content"), content);
    return writeSettings();
}

bool SettingsManager::removeSitePermissionRule(const QString &permissionType, const QString &origin)
{
    const QString normalizedPermissionType = permissionType.trimmed().toLower();
    const QString normalizedOrigin = normalizeOrigin(QUrl(origin.trimmed()));
    if (!isSupportedPermissionType(normalizedPermissionType) || normalizedOrigin.isEmpty())
        return false;

    QJsonObject content = m_settings.value(QStringLiteral("content")).toObject();
    QJsonObject rules = content.value(QStringLiteral("sitePermissionRules")).toObject();
    QJsonArray entries = rules.value(normalizedPermissionType).toArray();
    QJsonArray updatedEntries;
    bool removed = false;

    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("origin")).toString() == normalizedOrigin) {
            removed = true;
            continue;
        }
        updatedEntries.append(entry);
    }

    if (!removed)
        return false;

    rules.insert(normalizedPermissionType, updatedEntries);
    content.insert(QStringLiteral("sitePermissionRules"), rules);
    m_settings.insert(QStringLiteral("content"), content);
    return writeSettings();
}

QString SettingsManager::sitePermissionRule(const QString &permissionType, const QUrl &origin) const
{
    const QString normalizedPermissionType = permissionType.trimmed().toLower();
    const QString normalizedOrigin = normalizeOrigin(origin);
    if (!isSupportedPermissionType(normalizedPermissionType) || normalizedOrigin.isEmpty())
        return {};

    const QJsonObject rules = m_settings.value(QStringLiteral("content"))
        .toObject()
        .value(QStringLiteral("sitePermissionRules"))
        .toObject();
    const QJsonArray entries = rules.value(normalizedPermissionType).toArray();
    for (const QJsonValue &value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("origin")).toString() == normalizedOrigin)
            return entry.value(QStringLiteral("policy")).toString();
    }

    return {};
}

bool SettingsManager::rememberSitePermissionRequest(const QString &permissionType, const QUrl &origin)
{
    const QString normalizedPermissionType = permissionType.trimmed().toLower();
    const QString normalizedOrigin = normalizeOrigin(origin);
    if (!isSupportedPermissionType(normalizedPermissionType) || normalizedOrigin.isEmpty())
        return false;

    if (!sitePermissionRule(normalizedPermissionType, origin).isEmpty())
        return false;

    return upsertSitePermissionRule(normalizedPermissionType, normalizedOrigin, QStringLiteral("ask"));
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
        activeDialogParent(),
        QStringLiteral("Choose Download Folder"),
        currentPath);

    if (selectedPath.isEmpty())
        return {};

    if (!updateSetting(QStringLiteral("downloads.defaultPath"), selectedPath))
        return {};

    return selectedPath;
}

bool SettingsManager::openDefaultAppsSettings()
{
#ifdef Q_OS_WIN
    const HINSTANCE result = ShellExecuteW(nullptr,
                                           L"open",
                                           L"ms-settings:defaultapps",
                                           nullptr,
                                           nullptr,
                                           SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
#else
    return false;
#endif
}

QString SettingsManager::getDefaultBrowserStatus() const
{
#ifdef Q_OS_WIN
    const QFileInfo exeInfo(QCoreApplication::applicationFilePath());
    const QString currentExe = (exeInfo.canonicalFilePath().isEmpty() ? exeInfo.absoluteFilePath() : exeInfo.canonicalFilePath()).toLower();
    if (currentExe.isEmpty())
        return QStringLiteral("unknown");

    const auto statusForScheme = [&](const QString &scheme) -> QString {
        const QString userChoiceKey = QStringLiteral("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\")
            + scheme + QStringLiteral("\\UserChoice");
        const QString progId = readRegistryString(HKEY_CURRENT_USER, userChoiceKey, L"ProgId");
        if (progId.isEmpty())
            return QStringLiteral("unknown");

        const QString command = commandForProgId(progId);
        if (command.isEmpty())
            return QStringLiteral("unknown");

        return executablePathFromCommand(command) == currentExe
            ? QStringLiteral("default")
            : QStringLiteral("not-default");
    };

    const QString httpStatus = statusForScheme(QStringLiteral("http"));
    const QString httpsStatus = statusForScheme(QStringLiteral("https"));
    if (httpStatus == QLatin1String("default") && httpsStatus == QLatin1String("default"))
        return QStringLiteral("default");
    if (httpStatus == QLatin1String("unknown") || httpsStatus == QLatin1String("unknown"))
        return QStringLiteral("unknown");
    return QStringLiteral("not-default");
#else
    return QStringLiteral("unsupported");
#endif
}

bool SettingsManager::resetToDefaults()
{
    m_settings = builtInDefaults();
    if (!writeSettings())
        return false;

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

    if (userSettings != m_settings)
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
            { QStringLiteral("profileName"), QStringLiteral("Ghost User") },
            { QStringLiteral("newTabModules"), QJsonObject {
                { QStringLiteral("weather"), true },
                { QStringLiteral("shortcuts"), true },
                { QStringLiteral("briefing"), true },
                { QStringLiteral("focus"), true },
            } },
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
            { QStringLiteral("youtubeShortsAsNormalVideos"), true },
            { QStringLiteral("siteSettings"), QJsonObject {
                { QStringLiteral("javascript"), QStringLiteral("allow") },
                { QStringLiteral("popups"), QStringLiteral("block") },
                { QStringLiteral("notifications"), QStringLiteral("ask") },
                { QStringLiteral("location"), QStringLiteral("ask") },
                { QStringLiteral("camera"), QStringLiteral("ask") },
                { QStringLiteral("microphone"), QStringLiteral("ask") },
            } },
            { QStringLiteral("sitePermissionRules"), QJsonObject {
                { QStringLiteral("notifications"), QJsonArray {} },
                { QStringLiteral("location"), QJsonArray {} },
                { QStringLiteral("camera"), QJsonArray {} },
                { QStringLiteral("microphone"), QJsonArray {} },
            } },
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
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "[SettingsManager] writeJsonFile OPEN FAILED:" << path << file.errorString();
        return false;
    }

    const qint64 bytes = file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    qDebug() << "[SettingsManager] writeJsonFile wrote" << bytes << "bytes to" << path;
    return true;
}

bool SettingsManager::writeSettings()
{
    qDebug() << "[SettingsManager] writeSettings -> path:" << userSettingsPath();
    if (!writeJsonFile(userSettingsPath(), m_settings)) {
        qDebug() << "[SettingsManager] writeSettings FAILED";
        return false;
    }

    emit settingsChanged(getSettingsJson());
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

QString SettingsManager::normalizeOrigin(const QUrl &url)
{
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty())
        return {};

    QUrl normalized(url);
    normalized.setPath(QString());
    normalized.setQuery(QString());
    normalized.setFragment(QString());
    normalized.setUserInfo(QString());

    QString origin = normalized.toString(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo);
    if (origin.endsWith(QLatin1Char('/')))
        origin.chop(1);
    return origin;
}

bool SettingsManager::isSupportedPermissionType(const QString &permissionType)
{
    static const QStringList supportedTypes {
        QStringLiteral("notifications"),
        QStringLiteral("location"),
        QStringLiteral("camera"),
        QStringLiteral("microphone"),
    };
    return supportedTypes.contains(permissionType);
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