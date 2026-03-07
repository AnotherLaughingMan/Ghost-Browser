#include <QApplication>
#include <QByteArray>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    // Chromium perf flags — must be set before QApplication
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--enable-gpu-rasterization "
            "--enable-zero-copy "
            "--ignore-gpu-blocklist "
            "--enable-oop-rasterization "
            "--num-raster-threads=4 "
            "--disable-gpu-driver-bug-workarounds "
            "--enable-accelerated-video-decode "
            "--enable-features=BackForwardCache,DirectCompositionVideoOverlays");

    QApplication app(argc, argv);
    app.setApplicationName("Ghost Browser");
    app.setOrganizationName("Ghost");
    app.setApplicationVersion("0.1.1");

    MainWindow window;
    window.show();

    return app.exec();
}
