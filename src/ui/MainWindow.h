#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QPoint>
#include <QWebEnginePage>

QT_BEGIN_NAMESPACE
class QHBoxLayout;
class QVBoxLayout;
class QLineEdit;
class QStackedWidget;
class QTabBar;
class QToolButton;
class QWebEngineDownloadRequest;
class QWebEngineProfile;
class QWebEngineView;
class QWidget;
QT_END_NAMESPACE

class CookieManager;
class GhostRequestInterceptor;
class HistoryManager;
class SettingsManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private slots:
    void addNewTab();
    void closeTab(int index);
    void switchTab(int index);
    void navigateToUrl();
    void navigateBack();
    void navigateForward();
    void reloadPage();
    void goHome();
    void updateUrlBar(const QUrl &url);
    void updateTabTitle(const QString &title);
    void openSettings();
    void openCodecTest();
    void onMinimize();
    void onMaximizeRestore();
    void onClose();

private:
    void addTab(const QUrl &url, const QString &label = QStringLiteral("New Tab"));
    void buildTitleBar();
    void buildNavBar();
    void buildBookmarksBar();
    void buildContentArea();
    void applyStyles();
    void applyAppearanceSettings();
    void applyContentSettings();
    void applyPrivacySettings();
    void applyDownloadSettings();
    void applySystemSettings();
    void configureProfile();
    void applyViewSettings(QWebEngineView *view);
    void applyPerViewContentRules(QWebEngineView *view);
    void applyFeaturePermission(QWebEnginePage *page, const QUrl &origin, QWebEnginePage::Feature feature);
    void refreshTabPresentation(QWebEngineView *view);
    void attachSettingsBridge(QWebEngineView *view);
    void handleDownloadRequested(QWebEngineDownloadRequest *download);
    void clearBrowsingDataIfNeeded();
    void saveSessionState() const;
    bool restoreSessionState();
    void refreshIcons();
    void trackMouseForResize(QWidget *widget);
    Qt::Edges resizeEdgesForGlobalPos(const QPoint &globalPos) const;
    void updateResizeCursor(const QPoint &globalPos);

    QWebEngineView *createWebView(const QUrl &url);
    QWebEngineView *currentWebView() const;
    QUrl homePageUrl() const;
    QUrl newTabUrl() const;
    QUrl startupPageUrl() const;
    QUrl searchUrlForQuery(const QString &query) const;
    QUrl resolveInternalUrl(const QString &page) const;
    QUrl normalizedYouTubeUrl(const QUrl &url) const;
    QString sessionStatePath() const;
    QString siteSettingValue(const QString &path, const QString &fallback) const;
    int tabIndexForView(QWebEngineView *view) const;
    bool isSettingsUrl(const QUrl &url) const;
    bool looksLikeUrl(const QString &input) const;
    QString iconPath(const QString &name) const;

    // Title bar
    QWidget      *m_titleBar      = nullptr;
    QTabBar      *m_tabBar        = nullptr;
    QWidget      *m_dragSpacer    = nullptr;
    QToolButton  *m_newTabBtn     = nullptr;
    QToolButton  *m_minimizeBtn   = nullptr;
    QToolButton  *m_maximizeBtn   = nullptr;
    QToolButton  *m_closeBtn      = nullptr;

    // Navigation bar
    QWidget      *m_navBar        = nullptr;
    QWidget      *m_bookmarksBar  = nullptr;
    QLineEdit    *m_urlBar        = nullptr;
    QToolButton  *m_backBtn       = nullptr;
    QToolButton  *m_forwardBtn    = nullptr;
    QToolButton  *m_reloadBtn     = nullptr;
    QToolButton  *m_homeBtn       = nullptr;
    QToolButton  *m_menuBtn       = nullptr;

    // Content
    QStackedWidget *m_pageStack   = nullptr;
    SettingsManager *m_settings   = nullptr;
    QWebEngineProfile *m_profile  = nullptr;
    GhostRequestInterceptor *m_requestInterceptor = nullptr;
    HistoryManager *m_history = nullptr;
    CookieManager *m_cookies = nullptr;

    // State
    bool  m_darkMode = true;
    bool  m_dragging = false;
    bool  m_trackingResize = false;
    QPoint m_dragPos;
};
