#include <QApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>
#include "ui/MainWindow.h"

static QFile *s_logFile = nullptr;

static void fileMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(ctx);
    if (!s_logFile)
        return;
    const char *tag = "DEBUG";
    switch (type) {
    case QtWarningMsg:  tag = "WARN"; break;
    case QtCriticalMsg: tag = "CRIT"; break;
    case QtFatalMsg:    tag = "FATAL"; break;
    default: break;
    }
    QTextStream out(s_logFile);
    out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
        << " [" << tag << "] " << msg << "\n";
    out.flush();
}

static bool readHardwareAccelSetting()
{
    // Read user settings before QApplication exists to decide GPU flags.
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty())
        return true;

    QFile file(QDir(configDir).filePath(QStringLiteral("settings.json")));
    if (!file.open(QIODevice::ReadOnly))
        return true;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return true;

    const QJsonObject system = doc.object().value(QStringLiteral("system")).toObject();
    const QJsonValue val = system.value(QStringLiteral("hardwareAcceleration"));
    return val.isUndefined() ? true : val.toBool(true);
}

int main(int argc, char *argv[])
{
    const bool hwAccel = readHardwareAccelSetting();

    if (hwAccel) {
        // Chromium perf flags — must be set before QApplication.
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
                "--enable-gpu-rasterization "
                "--enable-zero-copy "
                "--ignore-gpu-blocklist "
                "--enable-oop-rasterization "
                "--num-raster-threads=4 "
                "--disable-gpu-driver-bug-workarounds "
                "--enable-accelerated-video-decode "
                "--enable-features=BackForwardCache,DirectCompositionVideoOverlays");
    } else {
        // Software rendering — disable GPU compositing entirely.
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
                "--disable-gpu "
                "--disable-gpu-compositing "
                "--enable-features=BackForwardCache");
        QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL, true);
    }

    QApplication app(argc, argv);
    app.setApplicationName("Ghost Browser");
    app.setOrganizationName("Ghost");
    app.setApplicationVersion("0.1.2");

    // Redirect qDebug/qWarning to a log file for diagnostics.
    const QString logPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                                .filePath(QStringLiteral("ghost_debug.log"));
    QDir().mkpath(QFileInfo(logPath).absolutePath());
    static QFile logFile(logPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        s_logFile = &logFile;
        qInstallMessageHandler(fileMessageHandler);
    }

    MainWindow window;
    window.show();

    return app.exec();
}
