#pragma once

#include <QByteArray>
#include <QEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMoveEvent>
#include <QPoint>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWebEnginePage>

QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QDockWidget;
class QHBoxLayout;
class QLabel;
class QMainWindow;
class QVBoxLayout;
class QLineEdit;
class QShortcut;
class QStackedWidget;
class QTabBar;
class QToolButton;
class QWebEngineDownloadRequest;
class QWebEngineProfile;
class QWebEngineView;
class QWidget;
QT_END_NAMESPACE

class CookieManager;
class BookmarkManager;
class GhostRequestInterceptor;
class HistoryManager;
class ProtectionDiagnostics;
class SettingsManager;
class StatusBubbleWidget;
class WeatherService;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
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
    void toggleDevTools();
    void addCurrentPageBookmark();
    void editCurrentPageBookmark();
    void removeCurrentPageBookmark();
    void importBookmarks();
    void exportBookmarks();
    void onMinimize();
    void onMaximizeRestore();
    void onClose();

private:
    enum class DevToolsPlacement {
        BottomDock,
        LeftDock,
        RightDock,
        Detached
    };

    void addTab(const QUrl &url, const QString &label = QStringLiteral("New Tab"));
    void buildTitleBar();
    void buildNavBar();
    void buildBookmarksBar();
    void refreshBookmarksBar();
    void buildStatusBar();
    void buildContentArea();
    void applyStyles();
    void applyAppearanceSettings();
    void applyContentSettings();
    void applyProtectionSettings();
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
    void saveDevToolsState();
    void restoreDevToolsState();
    void openSettingsFragment(const QString &fragment = QString());
    void refreshBookmarkMenuActions();
    void ensureDevToolsView();
    void ensureDevToolsDock();
    void ensureDevToolsWindow();
    void attachDevToolsView(QWidget *container);
    void setDevToolsPlacement(DevToolsPlacement placement);
    void updateDevToolsTarget();
    void updateDevToolsActions();
    void saveWindowPlacement() const;
    void restoreWindowPlacement();
    void refreshIcons();
    void refreshStatusBar();
    void showSiteInfoPopup();
    void refreshSiteInfoIcon();
    void setBrowserChromeVisible(bool visible);
    void enterVideoFullScreen(QWebEngineView *view);
    void exitVideoFullScreen();
    void ensureFullScreenExitButton();
    void ensureFullScreenExitHint();
    void showFullScreenExitButton();
    void showFullScreenExitHint();
    void scheduleFullScreenExitButtonHide();
    void hideFullScreenExitButton();
    void updateFullScreenExitButtonGeometry();
    void updateFullScreenExitHintGeometry();
    bool handlePlaybackShortcut(QKeyEvent *event, QWebEngineView *view);
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
    QString windowPlacementPath() const;
    QString devtoolsStatePath() const;
    QString siteSettingValue(const QString &path, const QString &fallback) const;
    QString permissionPolicyForOrigin(const QString &permissionType, const QUrl &origin, const QString &defaultPolicy) const;
    QWebEnginePage::PermissionPolicy promptForPermissionDecision(const QUrl &origin, const QStringList &permissionTypes, bool *rememberChoice);
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
    StatusBubbleWidget *m_statusBar = nullptr;
    QLineEdit    *m_urlBar        = nullptr;
    QToolButton  *m_backBtn       = nullptr;
    QToolButton  *m_forwardBtn    = nullptr;
    QToolButton  *m_reloadBtn     = nullptr;
    QToolButton  *m_homeBtn       = nullptr;
    QToolButton  *m_siteInfoBtn   = nullptr;
    QToolButton  *m_menuBtn       = nullptr;
    QAction      *m_devToolsAction = nullptr;
    QAction      *m_devToolsDockBottomAction = nullptr;
    QAction      *m_devToolsDockLeftAction = nullptr;
    QAction      *m_devToolsDockRightAction = nullptr;
    QAction      *m_devToolsDetachedAction = nullptr;
    QAction      *m_addBookmarkAction = nullptr;
    QAction      *m_editBookmarkAction = nullptr;
    QAction      *m_removeBookmarkAction = nullptr;
    QToolButton  *m_fullScreenExitBtn = nullptr;
    QLabel       *m_fullScreenExitHintLabel = nullptr;

    // Content
    QWidget *m_contentArea = nullptr;
    QStackedWidget *m_pageStack   = nullptr;
    SettingsManager *m_settings   = nullptr;
    BookmarkManager *m_bookmarks  = nullptr;
    QWebEngineProfile *m_profile  = nullptr;
    GhostRequestInterceptor *m_requestInterceptor = nullptr;
    HistoryManager *m_history = nullptr;
    CookieManager *m_cookies = nullptr;
    ProtectionDiagnostics *m_protectionDiagnostics = nullptr;
    WeatherService *m_weatherService = nullptr;

    // State
    bool  m_darkMode = true;
    bool  m_dragging = false;
    bool  m_trackingResize = false;
    bool  m_restoringWindowPlacement = false;
    bool  m_restoringDevToolsState = false;
    bool  m_windowPlacementReady = false;
    QRect m_preMaximizeGeometry;   // geometry saved before maximizing, for reliable restore
    bool  m_browserChromeVisible = true;
    bool  m_wasMaximizedBeforeVideoFullScreen = false;
    QString m_hoveredLink;
    QPointer<QWebEngineView> m_fullScreenView;
    QPointer<QDockWidget> m_devToolsDock;
    QPointer<QMainWindow> m_devToolsWindow;
    QPointer<QWebEngineView> m_devToolsView;
    QShortcut *m_fullScreenExitShortcut = nullptr;
    QShortcut *m_devToolsShortcut = nullptr;
    QShortcut *m_devToolsAlternateShortcut = nullptr;
    QTimer *m_fullScreenExitHideTimer = nullptr;
    QGraphicsOpacityEffect *m_fullScreenExitButtonOpacityEffect = nullptr;
    QPropertyAnimation *m_fullScreenExitButtonOpacityAnimation = nullptr;
    QGraphicsOpacityEffect *m_fullScreenExitHintOpacityEffect = nullptr;
    QPropertyAnimation *m_fullScreenExitHintOpacityAnimation = nullptr;
    QPoint m_dragPos;
    DevToolsPlacement m_devToolsPlacement = DevToolsPlacement::BottomDock;
};
