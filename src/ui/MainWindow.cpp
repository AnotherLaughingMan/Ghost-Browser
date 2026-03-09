#include "MainWindow.h"

#include "browser/FingerprintingProtection.h"
#include "browser/GhostRequestInterceptor.h"
#include "core/CookieManager.h"
#include "core/HistoryManager.h"
#include "core/ProtectionDiagnostics.h"
#include "core/SettingsManager.h"
#include "core/WeatherService.h"

#include <QAction>
#include <QAbstractButton>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QRegularExpression>
#include <QScreen>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QPushButton>
#include <QStringList>
#include <QStyleHints>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWindow>
#include <QWebChannel>
#include <QWebEngineCookieStore>
#include <QWebEngineDownloadRequest>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace {

QString permissionTypeLabel(const QString &permissionType)
{
    if (permissionType == QLatin1String("notifications"))
        return QStringLiteral("notifications");
    if (permissionType == QLatin1String("location"))
        return QStringLiteral("your location");
    if (permissionType == QLatin1String("camera"))
        return QStringLiteral("your camera");
    if (permissionType == QLatin1String("microphone"))
        return QStringLiteral("your microphone");
    return permissionType;
}

QString joinPermissionLabels(const QStringList &permissionTypes)
{
    QStringList labels;
    for (const QString &permissionType : permissionTypes)
        labels.append(permissionTypeLabel(permissionType));

    if (labels.isEmpty())
        return QStringLiteral("this feature");
    if (labels.size() == 1)
        return labels.front();
    if (labels.size() == 2)
        return labels.at(0) + QStringLiteral(" and ") + labels.at(1);

    const QString last = labels.takeLast();
    return labels.join(QStringLiteral(", ")) + QStringLiteral(", and ") + last;
}

QWebEnginePage::PermissionPolicy policyForValue(const QString &value)
{
    if (value == QLatin1String("allow"))
        return QWebEnginePage::PermissionGrantedByUser;
    if (value == QLatin1String("block"))
        return QWebEnginePage::PermissionDeniedByUser;
    return QWebEnginePage::PermissionUnknown;
}

QString overlayScrollbarScriptSource(bool darkMode)
{
        const QString thumb = darkMode ? QStringLiteral("rgba(164, 177, 217, 0.62)") : QStringLiteral("rgba(107, 120, 151, 0.55)");
        const QString thumbHover = darkMode ? QStringLiteral("rgba(199, 210, 243, 0.84)") : QStringLiteral("rgba(75, 88, 119, 0.78)");

        return QStringLiteral(R"JS((() => {
    const root = document.documentElement;
    if (!root) return;

    const styleId = '__ghost_overlay_scrollbars';
    let style = document.getElementById(styleId);
    if (!style) {
        style = document.createElement('style');
        style.id = styleId;
        document.head.appendChild(style);
    }

    style.textContent = `
        html, body {
            overflow: overlay !important;
            scrollbar-gutter: auto !important;
        }
        html {
            --ghost-scrollbar-size: 12px;
            --ghost-scrollbar-thumb-inset: 3px;
            --ghost-scrollbar-thumb: %1;
            --ghost-scrollbar-thumb-hover: %2;
        }
        html.ghost-scrollbar-hover {
            --ghost-scrollbar-thumb-inset: 1px;
        }
        ::-webkit-scrollbar {
            width: var(--ghost-scrollbar-size);
            height: var(--ghost-scrollbar-size);
        }
        ::-webkit-scrollbar-track {
            background: transparent;
            margin: 4px 0;
        }
        ::-webkit-scrollbar-thumb {
            background: var(--ghost-scrollbar-thumb);
            border-radius: 999px;
            border: var(--ghost-scrollbar-thumb-inset) solid transparent;
            background-clip: padding-box;
            min-height: 36px;
            transition: border 140ms ease, background-color 140ms ease;
        }
        ::-webkit-scrollbar-thumb:hover {
            background: var(--ghost-scrollbar-thumb-hover);
            background-clip: padding-box;
        }
        ::-webkit-scrollbar-corner {
            background: transparent;
        }
    `;

    if (!window.__ghostScrollbarHoverBound) {
        const setHoverState = (event) => {
            const edgeThreshold = 28;
            const nearRight = window.innerWidth - event.clientX <= edgeThreshold;
            const nearBottom = window.innerHeight - event.clientY <= edgeThreshold;
            root.classList.toggle('ghost-scrollbar-hover', nearRight || nearBottom);
        };

        window.addEventListener('mousemove', setHoverState, { passive: true });
        window.addEventListener('mouseleave', () => {
            root.classList.remove('ghost-scrollbar-hover');
        }, { passive: true });
        window.__ghostScrollbarHoverBound = true;
    }
})();)JS").arg(thumb, thumbHover);
}

QString accessibilityScriptSource(bool highContrast, bool darkMode)
{
    return QStringLiteral(R"JS((() => {
    const root = document.documentElement;
    if (!root) return;

    const styleId = '__ghost_accessibility_overrides';
    let style = document.getElementById(styleId);
    if (!style) {
        style = document.createElement('style');
        style.id = styleId;
        document.head.appendChild(style);
    }

    root.classList.toggle('ghost-high-contrast', %1);

    style.textContent = `
        html.ghost-high-contrast {
            color-scheme: %2;
            filter: contrast(%3) saturate(%4) brightness(%5);
        }
        html.ghost-high-contrast * {
            text-shadow: none !important;
            box-shadow: none !important;
        }
        html.ghost-high-contrast a {
            text-decoration-thickness: 2px !important;
            text-underline-offset: 0.18em !important;
        }
        html.ghost-high-contrast input,
        html.ghost-high-contrast textarea,
        html.ghost-high-contrast select,
        html.ghost-high-contrast button,
        html.ghost-high-contrast [role="button"],
        html.ghost-high-contrast [tabindex] {
            outline: 1px solid rgba(255, 196, 64, 0.35) !important;
            outline-offset: 0 !important;
        }
        html.ghost-high-contrast *:focus-visible {
            outline: 3px solid rgba(255, 196, 64, 0.95) !important;
            outline-offset: 2px !important;
        }
        html.ghost-high-contrast ::selection {
            background: rgba(255, 196, 64, 0.35) !important;
            color: inherit !important;
        }
    `;
})();)JS")
        .arg(highContrast ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(darkMode ? QStringLiteral("dark") : QStringLiteral("light"))
        .arg(darkMode ? QStringLiteral("1.26") : QStringLiteral("1.18"))
        .arg(darkMode ? QStringLiteral("0.82") : QStringLiteral("0.9"))
        .arg(darkMode ? QStringLiteral("1.04") : QStringLiteral("1.02"));
}

QString displayUrlForUi(const QUrl &url)
{
    if (!url.isValid())
        return QStringLiteral("Ready");

    if (url.scheme() == QLatin1String("qrc")) {
        if (url.path() == QLatin1String("/pages/newtab.html"))
            return QStringLiteral("ghost://newtab");
        if (url.path() == QLatin1String("/pages/settings.html"))
            return QStringLiteral("ghost://settings");
        if (url.path() == QLatin1String("/pages/codec-test.html"))
            return QStringLiteral("ghost://codec-test");
    }

    return url.toString();
}

bool isInternalGhostPage(const QUrl &url)
{
    return url.scheme() == QLatin1String("qrc")
        && url.path().startsWith(QLatin1String("/pages/"));
}

QString statusStateForUrl(const QUrl &url)
{
    if (!url.isValid())
        return QStringLiteral("Idle");
    if (isInternalGhostPage(url))
        return QStringLiteral("Internal Page");
    if (url.scheme() == QLatin1String("https"))
        return QStringLiteral("Secure");
    if (url.scheme() == QLatin1String("http"))
        return QStringLiteral("Insecure");

    return url.scheme().isEmpty()
        ? QStringLiteral("Idle")
        : url.scheme().toUpper();
}

}

// ── Construction ──────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    resize(1280, 800);
    m_settings = new SettingsManager(this);
    m_history  = new HistoryManager(this);
    m_protectionDiagnostics = new ProtectionDiagnostics(this);
    m_weatherService = new WeatherService(this);
    m_profile = new QWebEngineProfile(QStringLiteral("Ghost"), this);
    m_requestInterceptor = new GhostRequestInterceptor(m_settings, m_protectionDiagnostics, this);
    connect(m_settings, &SettingsManager::settingsChanged,
            m_requestInterceptor, &GhostRequestInterceptor::refreshSettings);
    configureProfile();
    m_profile->setUrlRequestInterceptor(m_requestInterceptor);
    m_cookies = new CookieManager(m_profile->cookieStore(), m_profile->persistentStoragePath(), this);
    connect(m_profile, &QWebEngineProfile::downloadRequested,
            this, &MainWindow::handleDownloadRequested);

    auto *central = new QWidget(this);
    auto *root    = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    buildTitleBar();
    buildNavBar();
    buildBookmarksBar();
    buildContentArea();
    buildStatusBar();

    root->addWidget(m_titleBar);
    root->addWidget(m_navBar);
    root->addWidget(m_bookmarksBar);
    root->addWidget(m_contentArea, 1);

    setCentralWidget(central);
    trackMouseForResize(this);
    trackMouseForResize(central);
    trackMouseForResize(m_titleBar);
    trackMouseForResize(m_tabBar);
    trackMouseForResize(m_navBar);
    trackMouseForResize(m_bookmarksBar);
    trackMouseForResize(m_urlBar);
    trackMouseForResize(m_contentArea);
    trackMouseForResize(m_pageStack);

    connect(m_settings, &SettingsManager::settingsChanged, this, [this](const QString &) {
        applyAppearanceSettings();
        applyContentSettings();
        applyProtectionSettings();
        applyPrivacySettings();
        applyDownloadSettings();
        applySystemSettings();
        saveSessionState();
    });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) {
                if (m_settings->value(QStringLiteral("appearance.theme")).toString() == QLatin1String("system"))
                    applyAppearanceSettings();
            });
    connect(m_settings, &SettingsManager::clearBrowsingDataRequested,
            this, [this]() {
                if (!m_profile)
                    return;

                m_profile->cookieStore()->deleteAllCookies();
                m_profile->clearHttpCache();
                m_profile->clearAllVisitedLinks();
                if (m_history)
                    m_history->clear();
            });

    applyAppearanceSettings();
    applyContentSettings();
    applyProtectionSettings();
    applyPrivacySettings();
    applyDownloadSettings();
    applySystemSettings();
    applyStyles();
    restoreWindowPlacement();

    auto *tabStateTimer = new QTimer(this);
    tabStateTimer->setInterval(750);
    connect(tabStateTimer, &QTimer::timeout, this, [this]() {
        if (!m_pageStack)
            return;

        for (int i = 0; i < m_pageStack->count(); ++i) {
            if (auto *view = qobject_cast<QWebEngineView *>(m_pageStack->widget(i)))
                refreshTabPresentation(view);
        }
    });
    tabStateTimer->start();

    if (!restoreSessionState())
        addTab(startupPageUrl());

    applyAppearanceSettings();
    refreshStatusBar();
}

MainWindow::~MainWindow() = default;

// ── Title Bar ─────────────────────────────────────────────────

void MainWindow::buildTitleBar()
{
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(42);

    auto *layout = new QHBoxLayout(m_titleBar);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(0);

    // Tab bar
    m_tabBar = new QTabBar(m_titleBar);
    m_tabBar->setObjectName("tabBar");
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    m_tabBar->setIconSize(QSize(16, 16));
    m_tabBar->setUsesScrollButtons(true);

    connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::switchTab);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int, int) {
        saveSessionState();
    });

    // New-tab (+) button — sits immediately after the last tab
    m_newTabBtn = new QToolButton(m_titleBar);
    m_newTabBtn->setIcon(QIcon(iconPath("plus")));
    m_newTabBtn->setIconSize(QSize(14, 14));
    m_newTabBtn->setToolTip("New Tab");
    m_newTabBtn->setObjectName("newTabBtn");
    m_newTabBtn->setFixedSize(30, 30);
    connect(m_newTabBtn, &QToolButton::clicked, this, &MainWindow::addNewTab);

    // Drag spacer — captures mouse for window dragging
    m_dragSpacer = new QWidget(m_titleBar);
    m_dragSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_dragSpacer->setMouseTracking(true);
    m_dragSpacer->installEventFilter(this);

    layout->addWidget(m_tabBar);
    layout->addWidget(m_newTabBtn);
    layout->addWidget(m_dragSpacer, 1);

    // Window control buttons
    auto makeWinBtn = [&](const QString &icon, const QString &tip, const QString &objName) {
        auto *btn = new QToolButton(m_titleBar);
        btn->setIcon(QIcon(iconPath(icon)));
        btn->setIconSize(QSize(14, 14));
        btn->setToolTip(tip);
        btn->setObjectName(objName);
        btn->setFixedSize(46, 42);
        return btn;
    };

    m_minimizeBtn = makeWinBtn("minimize", "Minimize", "winMinBtn");
    m_maximizeBtn = makeWinBtn("maximize", "Maximize", "winMaxBtn");
    m_closeBtn    = makeWinBtn("x",        "Close",    "winCloseBtn");

    connect(m_minimizeBtn, &QToolButton::clicked, this, &MainWindow::onMinimize);
    connect(m_maximizeBtn, &QToolButton::clicked, this, &MainWindow::onMaximizeRestore);
    connect(m_closeBtn,    &QToolButton::clicked, this, &MainWindow::onClose);

    layout->addWidget(m_minimizeBtn);
    layout->addWidget(m_maximizeBtn);
    layout->addWidget(m_closeBtn);
}

// ── Navigation Bar ────────────────────────────────────────────

void MainWindow::buildNavBar()
{
    m_navBar = new QWidget(this);
    m_navBar->setObjectName("navBar");
    m_navBar->setFixedHeight(40);

    auto *layout = new QHBoxLayout(m_navBar);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);

    auto makeNavBtn = [&](const QString &icon, const QString &tip) {
        auto *btn = new QToolButton(m_navBar);
        btn->setIcon(QIcon(iconPath(icon)));
        btn->setIconSize(QSize(16, 16));
        btn->setToolTip(tip);
        btn->setObjectName("navBtn");
        btn->setFixedSize(28, 28);
        return btn;
    };

    m_backBtn    = makeNavBtn("arrow-left",  "Back");
    m_forwardBtn = makeNavBtn("arrow-right", "Forward");
    m_reloadBtn  = makeNavBtn("refresh-cw",  "Reload");
    m_homeBtn    = makeNavBtn("home",        "Home");

    connect(m_backBtn,    &QToolButton::clicked, this, &MainWindow::navigateBack);
    connect(m_forwardBtn, &QToolButton::clicked, this, &MainWindow::navigateForward);
    connect(m_reloadBtn,  &QToolButton::clicked, this, &MainWindow::reloadPage);
    connect(m_homeBtn,    &QToolButton::clicked, this, &MainWindow::goHome);

    m_urlBar = new QLineEdit(m_navBar);
    m_urlBar->setObjectName("urlBar");
    m_urlBar->setPlaceholderText("Search or enter URL...");
    connect(m_urlBar, &QLineEdit::returnPressed, this, &MainWindow::navigateToUrl);

    m_menuBtn = makeNavBtn("menu", "Menu");
    m_menuBtn->setObjectName("menuBtn");
    m_menuBtn->setPopupMode(QToolButton::InstantPopup);

    auto *menu = new QMenu(m_menuBtn);
    menu->setObjectName("mainMenu");
    menu->addAction("New Tab",      this, &MainWindow::addNewTab);
    menu->addSeparator();
    menu->addAction("Settings",     this, &MainWindow::openSettings);
    menu->addAction("History",      this, []{});
    menu->addAction("Bookmarks",    this, []{});
    menu->addAction("Downloads",    this, []{});
    menu->addSeparator();
    menu->addAction("Codec Test",   this, &MainWindow::openCodecTest);
    menu->addSeparator();
    menu->addAction("Exit",         this, &MainWindow::onClose);
    m_menuBtn->setMenu(menu);

    layout->addWidget(m_backBtn);
    layout->addWidget(m_forwardBtn);
    layout->addWidget(m_reloadBtn);
    layout->addWidget(m_homeBtn);
    layout->addWidget(m_urlBar, 1);
    layout->addWidget(m_menuBtn);
}

void MainWindow::buildBookmarksBar()
{
    m_bookmarksBar = new QWidget(this);
    m_bookmarksBar->setObjectName("bookmarksBar");

    auto *layout = new QHBoxLayout(m_bookmarksBar);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    auto makeBookmark = [&](const QString &text, const std::function<void()> &handler) {
        auto *button = new QToolButton(m_bookmarksBar);
        button->setObjectName("bookmarkBtn");
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        connect(button, &QToolButton::clicked, this, handler);
        layout->addWidget(button);
        return button;
    };

    makeBookmark(QStringLiteral("Home"), [this]() { goHome(); });
    makeBookmark(QStringLiteral("Settings"), [this]() { openSettings(); });
    makeBookmark(QStringLiteral("Codec Test"), [this]() { openCodecTest(); });
    layout->addStretch(1);
}

void MainWindow::buildStatusBar()
{
    m_statusBar = new QWidget(m_contentArea);
    m_statusBar->setObjectName("statusBar");
    m_statusBar->setFixedHeight(28);
    m_statusBar->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_statusBar->setAttribute(Qt::WA_StyledBackground, true);
    m_statusBar->setAutoFillBackground(false);

    m_statusOverlayBackdrop = new QWidget(m_statusBar);
    m_statusOverlayBackdrop->setObjectName(QStringLiteral("statusOverlayBackdrop"));
    m_statusOverlayBackdrop->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_statusOverlayBackdrop->lower();

    auto *layout = new QHBoxLayout(m_statusBar);
    layout->setContentsMargins(12, 4, 12, 4);
    layout->setSpacing(8);

    m_statusLabel = new QLabel(QStringLiteral("Ready"), m_statusBar);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_statusStateLabel = new QLabel(QStringLiteral("Idle"), m_statusBar);
    m_statusStateLabel->setObjectName(QStringLiteral("statusStateLabel"));
    m_statusStateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(m_statusLabel, 1);
    layout->addWidget(m_statusStateLabel);

    m_statusBar->raise();
    updateStatusOverlayGeometry();
}

// ── Content ───────────────────────────────────────────────────

void MainWindow::buildContentArea()
{
    m_contentArea = new QWidget(this);
    m_contentArea->setObjectName("contentArea");

    auto *layout = new QVBoxLayout(m_contentArea);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_pageStack = new QStackedWidget(m_contentArea);
    m_pageStack->setObjectName("pageStack");
    layout->addWidget(m_pageStack, 1);
}

void MainWindow::addTab(const QUrl &url, const QString &label)
{
    auto *view = createWebView(url);
    m_pageStack->addWidget(view);
    const int idx = m_tabBar->addTab(label);
    refreshTabPresentation(view);
    m_tabBar->setCurrentIndex(idx);
    saveSessionState();

    if (url.scheme() == QLatin1String("qrc"))
        m_urlBar->clear();
    else
        m_urlBar->setText(url.toString());

    m_hoveredLink.clear();
    refreshStatusBar();
    ensureStatusOverlayOnTop();
}

// ── Styles ────────────────────────────────────────────────────

void MainWindow::applyStyles()
{
    const bool highContrast = m_settings && m_settings->value(QStringLiteral("accessibility.highContrast")).toBool();
    const QString background = m_darkMode
        ? (highContrast ? QStringLiteral("#101217") : QStringLiteral("#1E1E2E"))
        : (highContrast ? QStringLiteral("#FFFFFF") : QStringLiteral("#F3F5F9"));
    const QString surface = m_darkMode
        ? (highContrast ? QStringLiteral("#000000") : QStringLiteral("#2A2A3C"))
        : (highContrast ? QStringLiteral("#FFFFFF") : QStringLiteral("#FFFFFF"));
    const QString surfaceHover = m_darkMode
        ? (highContrast ? QStringLiteral("#1B1F27") : QStringLiteral("#333346"))
        : (highContrast ? QStringLiteral("#F2F6FF") : QStringLiteral("#E8ECF5"));
    const QString textPrimary = m_darkMode
        ? (highContrast ? QStringLiteral("#FFFFFF") : QStringLiteral("#E0E0E0"))
        : (highContrast ? QStringLiteral("#0A0C10") : QStringLiteral("#1F2430"));
    const QString textMuted = m_darkMode
        ? (highContrast ? QStringLiteral("#E6EAF4") : QStringLiteral("#AAA"))
        : (highContrast ? QStringLiteral("#1E2430") : QStringLiteral("#5E6573"));
    const QString border = m_darkMode
        ? (highContrast ? QStringLiteral("#F4F7FF") : QStringLiteral("#3A3A4A"))
        : (highContrast ? QStringLiteral("#212734") : QStringLiteral("#D9DFEA"));
    const QString accent = highContrast ? QStringLiteral("#FFC440") : QStringLiteral("#5B5FC7");

    setStyleSheet(QStringLiteral(R"(
        #titleBar {
            background: %1;
        }
        #tabBar {
            background: transparent;
        }
        #tabBar::tab {
            background: %1;
            color: %3;
            border: none;
            padding: 8px 18px;
            margin-right: 1px;
            min-height: 32px;
            min-width: 196px;
            max-width: 320px;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
        }
        #tabBar::tab:selected {
            background: %2;
            color: %4;
            border-bottom: 2px solid %5;
        }
        #tabBar::tab:hover:!selected {
            background: %6;
        }
        #tabBar::close-button {
            image: url(%7);
            subcontrol-position: right;
            padding: 2px;
        }
        #newTabBtn, #navBtn, #menuBtn {
            background: transparent;
            border: none;
            border-radius: 4px;
        }
        #tabAudioButton {
            background: transparent;
            border: none;
            border-radius: 3px;
            padding: 0px;
        }
        #tabAudioButton:hover {
            background: rgba(91,95,199,0.16);
        }
        #menuBtn::menu-indicator {
            image: none;
            width: 0px;
        }
        #newTabBtn:hover, #navBtn:hover, #menuBtn:hover {
            background: rgba(91,95,199,0.14);
        }
        #winMinBtn, #winMaxBtn {
            background: transparent;
            border: none;
        }
        #winMinBtn:hover, #winMaxBtn:hover {
            background: rgba(91,95,199,0.14);
        }
        #winCloseBtn {
            background: transparent;
            border: none;
        }
        #winCloseBtn:hover {
            background: #E81123;
        }
        #navBar {
            background: %1;
            border-bottom: 1px solid %8;
        }
        #bookmarksBar {
            background: %1;
            border-bottom: 1px solid %8;
        }
        #urlBar {
            background: %2;
            color: %4;
            border: 1px solid %8;
            border-radius: 6px;
            padding: 4px 12px;
            font-size: 13px;
            selection-background-color: %5;
        }
        #urlBar:focus {
            border-color: %5;
        }
        #bookmarkBtn {
            background: transparent;
            color: %4;
            border: 1px solid transparent;
            border-radius: 5px;
            padding: 4px 10px;
        }
        #bookmarkBtn:hover {
            background: %6;
            border-color: %8;
        }
        #pageStack {
            background: %1;
        }
        #statusBar {
            background: transparent;
        }
        #statusOverlayBackdrop {
            border-radius: 10px;
        }
        #statusLabel {
            color: %3;
            font-size: 11px;
            padding-right: 8px;
            background: transparent;
        }
        #statusStateLabel {
            color: %4;
            font-size: 11px;
            font-weight: 600;
            padding-left: 8px;
            border-left: 1px solid %8;
            background: transparent;
        }
        #mainMenu {
            background: %2;
            color: %4;
            border: 1px solid %8;
            border-radius: 6px;
            padding: 4px 0;
        }
        #mainMenu::item {
            padding: 8px 24px;
        }
        #mainMenu::item:selected {
            background: rgba(91,95,199,0.25);
        }
        #mainMenu::separator {
            height: 1px;
            background: %8;
            margin: 4px 8px;
        }
    )")
        .arg(background)
        .arg(surface)
        .arg(textMuted)
        .arg(textPrimary)
        .arg(accent)
        .arg(surfaceHover)
        .arg(iconPath(QStringLiteral("x")))
        .arg(border));
}

void MainWindow::applyAppearanceSettings()
{
    const QString theme = m_settings->value(QStringLiteral("appearance.theme")).toString();
    if (theme == QLatin1String("light")) {
        m_darkMode = false;
    } else if (theme == QLatin1String("system")) {
        m_darkMode = QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
    } else {
        m_darkMode = true;
    }

    if (m_bookmarksBar)
        m_bookmarksBar->setVisible(m_settings->value(QStringLiteral("appearance.showBookmarksBar")).toBool());

    refreshIcons();
    applyStyles();

    if (m_statusBar && m_statusOverlayBackdrop) {
        QString mode = m_settings->value(QStringLiteral("appearance.statusOverlayMode")).toString().trimmed();
        if (mode.isEmpty())
            mode = QStringLiteral("frosted");
        const int configuredOpacity = m_settings->value(QStringLiteral("appearance.statusOverlayOpacity")).toInt();
        const int opacity = qBound(0, configuredOpacity, 100);
        const bool isOff = mode == QLatin1String("off");
        m_statusBar->setVisible(!isOff);

        QColor fillColor = m_darkMode ? QColor(20, 25, 39) : QColor(255, 255, 255);
        QColor borderColor = m_darkMode ? QColor(156, 170, 211) : QColor(175, 184, 203);
        QString backdropStyle;
        auto *shadow = qobject_cast<QGraphicsDropShadowEffect *>(m_statusOverlayBackdrop->graphicsEffect());
        if (!shadow) {
            shadow = new QGraphicsDropShadowEffect(m_statusOverlayBackdrop);
            m_statusOverlayBackdrop->setGraphicsEffect(shadow);
        }

        if (mode == QLatin1String("transparent")) {
            fillColor.setAlphaF(opacity / 100.0);
            borderColor.setAlpha(0);
            backdropStyle = QStringLiteral(
                "background: %1; border: none; border-radius: 10px;")
                                .arg(fillColor.name(QColor::HexArgb));
            shadow->setEnabled(false);
        } else if (mode == QLatin1String("solid")) {
            fillColor.setAlpha(255);
            borderColor.setAlphaF(0.45);
            backdropStyle = QStringLiteral(
                "background: %1; border: 1px solid %2; border-radius: 10px;")
                                .arg(fillColor.name(QColor::HexArgb), borderColor.name(QColor::HexArgb));
            shadow->setEnabled(false);
        } else {
            // Frosted is tuned per theme so light mode stays airy while dark mode reads denser.
            const QColor topTint = m_darkMode
                ? QColor(214, 225, 248, qBound(22, opacity / 2 + 18, 54))
                : QColor(255, 255, 255, qBound(138, opacity + 92, 210));
            const QColor midTint = m_darkMode
                ? QColor(42, 52, 80, qBound(156, opacity * 2 + 72, 220))
                : QColor(244, 248, 255, qBound(170, opacity * 2 + 92, 234));
            const QColor bottomTint = m_darkMode
                ? QColor(14, 18, 30, qBound(184, opacity * 2 + 104, 236))
                : QColor(233, 240, 251, qBound(178, opacity * 2 + 108, 242));
            backdropStyle = QStringLiteral(
                "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "stop:0 %1, stop:0.42 %2, stop:1 %3); "
                "border: none; border-radius: 10px;")
                                .arg(topTint.name(QColor::HexArgb),
                                     midTint.name(QColor::HexArgb),
                                     bottomTint.name(QColor::HexArgb));
            shadow->setBlurRadius(m_darkMode ? 34.0 : 26.0);
            shadow->setOffset(0.0, m_darkMode ? 6.0 : 4.0);
            shadow->setColor(QColor(0, 0, 0, m_darkMode ? 132 : 52));
            shadow->setEnabled(true);
        }

        if (opacity == 0 && mode != QLatin1String("solid")) {
            fillColor.setAlpha(0);
            borderColor.setAlpha(0);
            backdropStyle = QStringLiteral("background: transparent; border: none; border-radius: 10px;");
            shadow->setEnabled(false);
        }

        m_statusOverlayBackdrop->setStyleSheet(backdropStyle);
        updateStatusOverlayGeometry();
        m_statusBar->raise();
    }

    for (int i = 0; i < m_pageStack->count(); ++i) {
        if (auto *view = qobject_cast<QWebEngineView *>(m_pageStack->widget(i))) {
            applyViewSettings(view);
            refreshTabPresentation(view);
        }
    }

    refreshStatusBar();
}

void MainWindow::applyContentSettings()
{
    for (int i = 0; i < m_pageStack->count(); ++i) {
        if (auto *view = qobject_cast<QWebEngineView *>(m_pageStack->widget(i))) {
            applyViewSettings(view);
            applyPerViewContentRules(view);
        }
    }
}

void MainWindow::applyProtectionSettings()
{
    if (!m_profile || !m_settings)
        return;

    const bool blockFingerprinting = m_settings->value(QStringLiteral("protection.blockFingerprinting")).toBool();
    setFingerprintingProtectionEnabled(m_profile, blockFingerprinting);

    if (!blockFingerprinting)
        return;

    const QString scriptSource = fingerprintingProtectionScriptSource();
    for (int i = 0; i < m_pageStack->count(); ++i) {
        if (auto *view = qobject_cast<QWebEngineView *>(m_pageStack->widget(i))) {
            if (!view->page())
                continue;

            view->page()->runJavaScript(scriptSource);
        }
    }
}

void MainWindow::applyPrivacySettings()
{
    if (!m_profile)
        return;

    const bool clearOnExit = m_settings->value(QStringLiteral("privacy.clearDataOnExit")).toBool();
    const bool blockThirdPartyCookies = m_settings->value(QStringLiteral("privacy.blockThirdPartyCookies")).toBool();
    m_profile->setPersistentCookiesPolicy(clearOnExit
        ? QWebEngineProfile::NoPersistentCookies
        : QWebEngineProfile::AllowPersistentCookies);

    if (m_cookies)
        m_cookies->setBlockThirdPartyCookies(blockThirdPartyCookies);
}

void MainWindow::applyDownloadSettings()
{
    if (!m_profile)
        return;

    const bool spellCheckEnabled = m_settings->value(QStringLiteral("languages.spellCheck")).toBool();
    m_profile->setSpellCheckEnabled(spellCheckEnabled);
    m_profile->setSpellCheckLanguages(QStringList { QStringLiteral("en-US") });
}

void MainWindow::applySystemSettings()
{
    const QString proxyMode = m_settings->value(QStringLiteral("system.proxyMode")).toString().trimmed();
    if (proxyMode == QLatin1String("none")) {
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    } else {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        QNetworkProxy::setApplicationProxy(QNetworkProxy());
    }

    for (int i = 0; i < m_pageStack->count(); ++i) {
        if (auto *view = qobject_cast<QWebEngineView *>(m_pageStack->widget(i)))
            applyViewSettings(view);
    }
}

void MainWindow::configureProfile()
{
    if (!m_profile)
        return;

    const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appDataRoot.isEmpty()) {
        QDir().mkpath(appDataRoot);
        QDir().mkpath(QDir(appDataRoot).filePath(QStringLiteral("cache")));
        m_profile->setPersistentStoragePath(appDataRoot);
        m_profile->setCachePath(QDir(appDataRoot).filePath(QStringLiteral("cache")));
    }

    m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    m_profile->setHttpCacheMaximumSize(512 * 1024 * 1024);
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
}

void MainWindow::applyViewSettings(QWebEngineView *view)
{
    if (!view)
        return;

    const int fontSize = m_settings->value(QStringLiteral("appearance.fontSize")).toInt();
    const int zoomLevel = m_settings->value(QStringLiteral("appearance.zoomLevel")).toInt();
    const bool autoplayEnabled = m_settings->value(QStringLiteral("content.autoplay")).toBool();
    const bool fullScreenEnabled = m_settings->value(QStringLiteral("content.fullScreenVideo")).toBool();
    const bool javascriptEnabled = siteSettingValue(QStringLiteral("content.siteSettings.javascript"), QStringLiteral("allow")) != QLatin1String("block");
    const bool popupsEnabled = siteSettingValue(QStringLiteral("content.siteSettings.popups"), QStringLiteral("block")) == QLatin1String("allow");
    const bool highContrast = m_settings->value(QStringLiteral("accessibility.highContrast")).toBool();
    const QColor backgroundColor = isInternalGhostPage(view->url())
        ? (m_darkMode
            ? (highContrast ? QColor(0x08, 0x0A, 0x10) : QColor(0x1E, 0x1E, 0x2E))
            : (highContrast ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xF3, 0xF5, 0xF9)))
        : QColor(Qt::white);

    view->settings()->setFontSize(QWebEngineSettings::DefaultFontSize, fontSize > 0 ? fontSize : 16);
    view->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, !autoplayEnabled);
    view->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, fullScreenEnabled);
    view->settings()->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
    view->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    view->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, javascriptEnabled);
    view->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, popupsEnabled);
    view->settings()->setAttribute(QWebEngineSettings::LinksIncludedInFocusChain, true);
    const bool hwAccel = m_settings->value(QStringLiteral("system.hardwareAcceleration")).toBool();
    view->settings()->setAttribute(QWebEngineSettings::WebGLEnabled, hwAccel);
    view->settings()->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, hwAccel);
    view->setZoomFactor((zoomLevel > 0 ? zoomLevel : 100) / 100.0);
    view->page()->setBackgroundColor(backgroundColor);
    view->page()->runJavaScript(overlayScrollbarScriptSource(m_darkMode));
    view->page()->runJavaScript(accessibilityScriptSource(highContrast, m_darkMode));
}

void MainWindow::refreshStatusBar()
{
    if (!m_statusLabel || !m_statusStateLabel)
        return;

    const QWebEngineView *view = currentWebView();
    const QUrl currentUrl = view ? view->url() : QUrl();

    m_statusLabel->setText(m_hoveredLink.isEmpty() ? displayUrlForUi(currentUrl) : m_hoveredLink);
    m_statusStateLabel->setText(statusStateForUrl(currentUrl));
    updateStatusOverlayGeometry();
    ensureStatusOverlayOnTop();
}

void MainWindow::ensureStatusOverlayOnTop()
{
    if (!m_statusBar || !m_statusBar->isVisible())
        return;

    m_statusBar->raise();
    QTimer::singleShot(0, this, [this]() {
        if (m_statusBar && m_statusBar->isVisible())
            m_statusBar->raise();
    });
}

void MainWindow::updateStatusOverlayGeometry()
{
    if (!m_contentArea || !m_pageStack || !m_statusBar || !m_statusOverlayBackdrop)
        return;

    if (!m_statusBar->isVisible())
        return;

    constexpr int leftInset = 10;
    constexpr int rightInset = 30;
    constexpr int bottomInset = 1;
    constexpr int overlayHeight = 28;
    const QRect stackRect = m_pageStack->geometry();
    const int overlayWidth = qMax(220, stackRect.width() - leftInset - rightInset);
    const QRect overlayRect(stackRect.x() + leftInset,
                            qMax(stackRect.y(), stackRect.bottom() - overlayHeight - bottomInset + 1),
                            overlayWidth,
                            overlayHeight);
    m_statusBar->setGeometry(overlayRect);
    m_statusOverlayBackdrop->setGeometry(m_statusBar->rect());
    m_statusOverlayBackdrop->lower();
    ensureStatusOverlayOnTop();
}

void MainWindow::applyPerViewContentRules(QWebEngineView *view)
{
    if (!view)
        return;

    const QUrl normalized = normalizedYouTubeUrl(view->url());
    if (normalized != view->url())
        view->setUrl(normalized);
}

void MainWindow::refreshIcons()
{
    if (m_newTabBtn)
        m_newTabBtn->setIcon(QIcon(iconPath(QStringLiteral("plus"))));
    if (m_minimizeBtn)
        m_minimizeBtn->setIcon(QIcon(iconPath(QStringLiteral("minimize"))));
    if (m_maximizeBtn) {
        const QString maximizeIcon = isMaximized() ? QStringLiteral("restore") : QStringLiteral("maximize");
        m_maximizeBtn->setIcon(QIcon(iconPath(maximizeIcon)));
    }
    if (m_closeBtn)
        m_closeBtn->setIcon(QIcon(iconPath(QStringLiteral("x"))));
    if (m_backBtn)
        m_backBtn->setIcon(QIcon(iconPath(QStringLiteral("arrow-left"))));
    if (m_forwardBtn)
        m_forwardBtn->setIcon(QIcon(iconPath(QStringLiteral("arrow-right"))));
    if (m_reloadBtn)
        m_reloadBtn->setIcon(QIcon(iconPath(QStringLiteral("refresh-cw"))));
    if (m_homeBtn)
        m_homeBtn->setIcon(QIcon(iconPath(QStringLiteral("home"))));
    if (m_menuBtn)
        m_menuBtn->setIcon(QIcon(iconPath(QStringLiteral("menu"))));
}

void MainWindow::trackMouseForResize(QWidget *widget)
{
    if (!widget)
        return;

    widget->setMouseTracking(true);
    widget->installEventFilter(this);
}

// ── Tab management ────────────────────────────────────────────

void MainWindow::addNewTab()
{
    addTab(newTabUrl());
    m_urlBar->clear();
    m_urlBar->setFocus();
}

void MainWindow::closeTab(int index)
{
    if (m_tabBar->count() <= 1) {
        close();
        return;
    }

    auto *widget = m_pageStack->widget(index);
    m_tabBar->removeTab(index);
    m_pageStack->removeWidget(widget);
    widget->deleteLater();
    saveSessionState();
    m_hoveredLink.clear();
    refreshStatusBar();
    ensureStatusOverlayOnTop();
}

void MainWindow::switchTab(int index)
{
    if (index < 0 || index >= m_pageStack->count())
        return;

    m_pageStack->setCurrentIndex(index);
    if (auto *view = currentWebView()) {
        const QUrl url = view->url();
        if (url.scheme() == QLatin1String("qrc"))
            m_urlBar->clear();
        else
            m_urlBar->setText(url.toString());
    }

    m_hoveredLink.clear();
    refreshStatusBar();
    ensureStatusOverlayOnTop();
    saveSessionState();
}

// ── Navigation ────────────────────────────────────────────────

void MainWindow::navigateToUrl()
{
    QString input = m_urlBar->text().trimmed();
    if (input.isEmpty())
        return;

    // Handle ghost:// internal pages
    if (input.startsWith(QLatin1String("ghost://"))) {
        QString page = input.mid(8); // strip "ghost://"
        QUrl internal = resolveInternalUrl(page);
        if (internal.isValid()) {
            if (auto *view = currentWebView()) {
                if (isSettingsUrl(internal))
                    attachSettingsBridge(view);
                view->setUrl(internal);
            }
            return;
        }
    }

    if (!input.contains(QLatin1String("://"))) {
        if (looksLikeUrl(input)) {
            input.prepend(QStringLiteral("https://"));
        } else {
            if (auto *view = currentWebView())
                view->setUrl(searchUrlForQuery(input));
            return;
        }
    }

    QUrl url(input, QUrl::TolerantMode);
    if (!url.isValid())
        return;

    if (auto *view = currentWebView())
        view->setUrl(url);
}

void MainWindow::navigateBack()
{
    if (auto *view = currentWebView())
        view->history()->back();
}

void MainWindow::navigateForward()
{
    if (auto *view = currentWebView())
        view->history()->forward();
}

void MainWindow::reloadPage()
{
    if (auto *view = currentWebView())
        view->reload();
}

void MainWindow::goHome()
{
    if (auto *view = currentWebView())
        view->setUrl(homePageUrl());
}

void MainWindow::openSettings()
{
    QUrl url = resolveInternalUrl(QStringLiteral("settings"));
    if (auto *view = currentWebView()) {
        attachSettingsBridge(view);
        view->setUrl(url);
    }
}

void MainWindow::openCodecTest()
{
    QUrl url = resolveInternalUrl(QStringLiteral("codec-test"));
    if (auto *view = currentWebView())
        view->setUrl(url);
}

// ── URL / title updates ──────────────────────────────────────

void MainWindow::updateUrlBar(const QUrl &url)
{
    auto *view = qobject_cast<QWebEngineView *>(sender());
    if (view && m_pageStack->currentWidget() == view) {
        if (url.scheme() == QLatin1String("qrc")) {
            // Map internal qrc pages back to ghost:// display URLs
            QString path = url.path();
            if (path == QLatin1String("/pages/newtab.html"))
                m_urlBar->clear();
            else if (path == QLatin1String("/pages/codec-test.html"))
                m_urlBar->setText(QStringLiteral("ghost://codec-test"));
            else if (path == QLatin1String("/pages/settings.html"))
                m_urlBar->setText(QStringLiteral("ghost://settings"));
            else
                m_urlBar->clear();
        } else {
            m_urlBar->setText(url.toString());
        }
    }
}

void MainWindow::updateTabTitle(const QString &title)
{
    auto *view = qobject_cast<QWebEngineView *>(sender());
    if (!view)
        return;

    Q_UNUSED(title);
    refreshTabPresentation(view);
}

// ── Window controls ──────────────────────────────────────────

void MainWindow::onMinimize()
{
    showMinimized();
}

void MainWindow::onMaximizeRestore()
{
    if (isMaximized()) {
        showNormal();
        m_maximizeBtn->setIcon(QIcon(iconPath("maximize")));
        m_maximizeBtn->setToolTip("Maximize");
    } else {
        showMaximized();
        m_maximizeBtn->setIcon(QIcon(iconPath("restore")));
        m_maximizeBtn->setToolTip("Restore");
    }

    saveWindowPlacement();
}

void MainWindow::onClose()
{
    close();
}

// ── Drag handling (title bar) ────────────────────────────────

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if ((obj == m_pageStack || obj == m_contentArea) && event->type() == QEvent::Resize) {
        updateStatusOverlayGeometry();
    }

#ifndef Q_OS_WIN
    if (!isMaximized() && !isFullScreen()) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *me = static_cast<QMouseEvent *>(event);
            updateResizeCursor(me->globalPosition().toPoint());
            break;
        }
        case QEvent::MouseButtonPress: {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                const Qt::Edges edges = resizeEdgesForGlobalPos(me->globalPosition().toPoint());
                if (edges != Qt::Edges() && windowHandle()) {
                    m_trackingResize = true;
                    windowHandle()->startSystemResize(edges);
                    return true;
                }
            }
            break;
        }
        case QEvent::MouseButtonRelease:
            m_trackingResize = false;
            break;
        case QEvent::Leave:
            if (!m_dragging && !m_trackingResize)
                unsetCursor();
            break;
        default:
            break;
        }
    } else if (event->type() == QEvent::MouseMove) {
        unsetCursor();
    }
#endif

    if (obj == m_dragSpacer) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton
                && resizeEdgesForGlobalPos(me->globalPosition().toPoint()) == Qt::Edges()) {
                m_dragging = true;
                m_dragPos  = me->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (isMaximized()) {
                // Adjust drag pos proportionally when leaving maximized
                const double ratio = static_cast<double>(me->globalPosition().toPoint().x()) / width();
                showNormal();
                m_dragPos = QPoint(static_cast<int>(width() * ratio), m_dragPos.y());
            }
            move(me->globalPosition().toPoint() - m_dragPos);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
            unsetCursor();
            return true;
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            onMaximizeRestore();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        refreshIcons();
        if (!m_restoringWindowPlacement)
            saveWindowPlacement();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowPlacement();
    saveSessionState();
    clearBrowsingDataIfNeeded();
    QMainWindow::closeEvent(event);
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);

    if (!m_restoringWindowPlacement && !isMaximized() && !isMinimized())
        saveWindowPlacement();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    updateStatusOverlayGeometry();

    if (!m_restoringWindowPlacement && !isMaximized() && !isMinimized())
        saveWindowPlacement();
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;

    const MSG *msg = static_cast<const MSG *>(message);
    if (!msg)
        return false;

    // Suppress background redraw during resize — eliminates edge-drag artifacts.
    if (msg->message == WM_NCCALCSIZE && msg->wParam == TRUE) {
        *result = 0;
        return true;
    }

    if (isMaximized() || isFullScreen() || msg->message != WM_NCHITTEST)
        return false;

    {
        const POINT cursor = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        const QRect frame = frameGeometry();
        constexpr int resizeMargin = 8;

        const bool onLeft = cursor.x >= frame.left() && cursor.x < frame.left() + resizeMargin;
        const bool onRight = cursor.x <= frame.right() && cursor.x > frame.right() - resizeMargin;
        const bool onTop = cursor.y >= frame.top() && cursor.y < frame.top() + resizeMargin;
        const bool onBottom = cursor.y <= frame.bottom() && cursor.y > frame.bottom() - resizeMargin;

        if (onTop && onLeft) {
            *result = HTTOPLEFT;
            return true;
        }
        if (onTop && onRight) {
            *result = HTTOPRIGHT;
            return true;
        }
        if (onBottom && onLeft) {
            *result = HTBOTTOMLEFT;
            return true;
        }
        if (onBottom && onRight) {
            *result = HTBOTTOMRIGHT;
            return true;
        }
        if (onLeft) {
            *result = HTLEFT;
            return true;
        }
        if (onRight) {
            *result = HTRIGHT;
            return true;
        }
        if (onTop) {
            *result = HTTOP;
            return true;
        }
        if (onBottom) {
            *result = HTBOTTOM;
            return true;
        }
    }

    return false;
}
#endif

// ── Helpers ──────────────────────────────────────────────────

QWebEngineView *MainWindow::createWebView(const QUrl &url)
{
    auto *view = new QWebEngineView(m_pageStack);
    auto *page = new QWebEnginePage(m_profile, view);
    view->setPage(page);

    // Dark background so blank/loading states aren't white flashes
    applyViewSettings(view);
    trackMouseForResize(view);

    if (isSettingsUrl(url) || url.path() == QLatin1String("/pages/newtab.html"))
        attachSettingsBridge(view);

    view->setUrl(normalizedYouTubeUrl(url));

    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &url) {
        const QUrl normalized = normalizedYouTubeUrl(url);
        if (normalized != url) {
            view->setUrl(normalized);
            return;
        }

        if (isSettingsUrl(url) || url.path() == QLatin1String("/pages/newtab.html"))
            attachSettingsBridge(view);

        applyViewSettings(view);
        refreshTabPresentation(view);
        updateUrlBar(url);
        if (m_pageStack->currentWidget() == view) {
            m_hoveredLink.clear();
            refreshStatusBar();
        }
        saveSessionState();
    });
    connect(view, &QWebEngineView::titleChanged,  this, &MainWindow::updateTabTitle);
    connect(page, &QWebEnginePage::featurePermissionRequested,
            this, [this, page](const QUrl &origin, QWebEnginePage::Feature feature) {
                applyFeaturePermission(page, origin, feature);
            });
    connect(page, &QWebEnginePage::iconChanged, this, [this, view](const QIcon &) {
        refreshTabPresentation(view);
    });
    connect(page, &QWebEnginePage::recentlyAudibleChanged, this, [this, view](bool) {
        refreshTabPresentation(view);
    });
    connect(page, &QWebEnginePage::audioMutedChanged, this, [this, view](bool) {
        refreshTabPresentation(view);
    });
    connect(page, &QWebEnginePage::linkHovered, this, [this, view](const QString &link) {
        if (m_pageStack->currentWidget() != view)
            return;

        m_hoveredLink = link.trimmed();
        if (!m_hoveredLink.isEmpty()) {
            const QUrl hoveredUrl(m_hoveredLink, QUrl::TolerantMode);
            if (hoveredUrl.isValid())
                m_hoveredLink = displayUrlForUi(hoveredUrl);
        }

        refreshStatusBar();
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
        if (ok && view) {
            view->page()->runJavaScript(overlayScrollbarScriptSource(m_darkMode));
            view->page()->runJavaScript(accessibilityScriptSource(
                m_settings->value(QStringLiteral("accessibility.highContrast")).toBool(),
                m_darkMode));
            refreshTabPresentation(view);
            m_history->recordVisit(view->url(), view->title());
            if (m_pageStack->currentWidget() == view)
                refreshStatusBar();
        }
    });

    return view;
}

void MainWindow::refreshTabPresentation(QWebEngineView *view)
{
    if (!view || !m_tabBar)
        return;

    const int idx = tabIndexForView(view);
    if (idx < 0)
        return;

    QWebEnginePage *page = view->page();
    QString label = view->title().trimmed();
    if (label.isEmpty())
        label = QStringLiteral("New Tab");

    QWidget *iconsWidget = m_tabBar->tabButton(idx, QTabBar::LeftSide);
    QLabel *faviconLabel = nullptr;
    QToolButton *audioButton = nullptr;

    if (!iconsWidget) {
        iconsWidget = new QWidget(m_tabBar);
        auto *iconsLayout = new QHBoxLayout(iconsWidget);
        iconsLayout->setContentsMargins(0, 0, 0, 0);
        iconsLayout->setSpacing(4);
        iconsWidget->setFixedSize(36, 16);

        faviconLabel = new QLabel(iconsWidget);
        faviconLabel->setObjectName(QStringLiteral("tabFaviconLabel"));
        faviconLabel->setFixedSize(16, 16);

        audioButton = new QToolButton(iconsWidget);
        audioButton->setObjectName(QStringLiteral("tabAudioButton"));
        audioButton->setAutoRaise(true);
        audioButton->setCursor(Qt::PointingHandCursor);
        audioButton->setFocusPolicy(Qt::NoFocus);
        audioButton->setFixedSize(16, 16);
        audioButton->setIconSize(QSize(14, 14));
        audioButton->setVisible(false);
        connect(audioButton, &QToolButton::clicked, this, [this, view]() {
            if (!view || !view->page())
                return;

            view->page()->setAudioMuted(!view->page()->isAudioMuted());
            refreshTabPresentation(view);
        });

        iconsLayout->addWidget(faviconLabel);
        iconsLayout->addWidget(audioButton);
        m_tabBar->setTabButton(idx, QTabBar::LeftSide, iconsWidget);
    } else {
        faviconLabel = iconsWidget->findChild<QLabel *>(QStringLiteral("tabFaviconLabel"));
        audioButton = iconsWidget->findChild<QToolButton *>(QStringLiteral("tabAudioButton"));
    }

    const QIcon pageIcon = (page && !page->icon().isNull())
        ? page->icon()
        : QIcon(QStringLiteral(":/app/ghost-small.ico"));

    if (faviconLabel)
        faviconLabel->setPixmap(pageIcon.pixmap(m_tabBar->iconSize()));

    const bool showAudioIndicator = page && (page->recentlyAudible() || page->isAudioMuted());
    if (audioButton) {
        if (showAudioIndicator) {
            const QString audioIconName = page->isAudioMuted()
                ? QStringLiteral("volume-x")
                : QStringLiteral("volume-2");
            audioButton->setIcon(QIcon(iconPath(audioIconName)));
            audioButton->setToolTip(page->isAudioMuted() ? QStringLiteral("Unmute Tab") : QStringLiteral("Mute Tab"));
            audioButton->setVisible(true);
            audioButton->raise();
            iconsWidget->updateGeometry();
        } else {
            audioButton->setIcon(QIcon());
            audioButton->setToolTip(QString());
            audioButton->setVisible(false);
            iconsWidget->updateGeometry();
        }
    }

    m_tabBar->setTabIcon(idx, QIcon());
    m_tabBar->setTabText(idx, label.left(34));
}

void MainWindow::attachSettingsBridge(QWebEngineView *view)
{
    if (!view || !view->page())
        return;

    if (view->page()->webChannel())
        return;

    auto *channel = new QWebChannel(view->page());
    channel->registerObject(QStringLiteral("ghostSettings"), m_settings);
    channel->registerObject(QStringLiteral("ghostHistory"), m_history);
    channel->registerObject(QStringLiteral("ghostCookies"), m_cookies);
    channel->registerObject(QStringLiteral("ghostProtection"), m_protectionDiagnostics);
    channel->registerObject(QStringLiteral("ghostWeather"), m_weatherService);
    view->page()->setWebChannel(channel);
}

QWebEngineView *MainWindow::currentWebView() const
{
    return qobject_cast<QWebEngineView *>(m_pageStack->currentWidget());
}

int MainWindow::tabIndexForView(QWebEngineView *view) const
{
    if (!view || !m_pageStack)
        return -1;

    return m_pageStack->indexOf(view);
}

QUrl MainWindow::homePageUrl() const
{
    const QUrl configured = m_settings->homePageUrl();
    if (configured.scheme() == QLatin1String("ghost"))
        return resolveInternalUrl(configured.toString().mid(8));
    return configured.isValid() ? configured : newTabUrl();
}

QUrl MainWindow::newTabUrl() const
{
    return QUrl(QStringLiteral("qrc:/pages/newtab.html"));
}

QUrl MainWindow::startupPageUrl() const
{
    const QString startup = m_settings->startupBehavior();
    if (startup == QLatin1String("specificPages"))
        return homePageUrl();
    return newTabUrl();
}

QString MainWindow::sessionStatePath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty())
        configDir = QCoreApplication::applicationDirPath();

    QDir dir(configDir);
    dir.mkpath(QStringLiteral("."));
    return dir.filePath(QStringLiteral("session.json"));
}

QString MainWindow::windowPlacementPath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty())
        configDir = QCoreApplication::applicationDirPath();

    QDir dir(configDir);
    dir.mkpath(QStringLiteral("."));
    return dir.filePath(QStringLiteral("window-state.json"));
}

void MainWindow::saveSessionState() const
{
    if (!m_settings || !m_pageStack || !m_tabBar)
        return;

    QJsonArray tabs;
    for (int i = 0; i < m_pageStack->count(); ++i) {
        auto *view = qobject_cast<QWebEngineView *>(m_pageStack->widget(i));
        if (!view)
            continue;

        const QUrl url = view->url().isValid() ? view->url() : newTabUrl();
        tabs.append(url.toString());
    }

    QJsonObject root {
        { QStringLiteral("currentIndex"), m_tabBar->currentIndex() },
        { QStringLiteral("tabs"), tabs },
    };

    QFile file(sessionStatePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool MainWindow::restoreSessionState()
{
    if (!m_settings || m_settings->startupBehavior() != QLatin1String("lastSession"))
        return false;

    QFile file(sessionStatePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const QJsonArray tabs = root.value(QStringLiteral("tabs")).toArray();
    if (tabs.isEmpty())
        return false;

    for (const QJsonValue &tabValue : tabs) {
        const QString urlText = tabValue.toString().trimmed();
        const QUrl url = urlText.isEmpty() ? newTabUrl() : QUrl(urlText);
        addTab(url.isValid() ? url : newTabUrl());
    }

    if (m_tabBar->count() > 0) {
        const int currentIndex = root.value(QStringLiteral("currentIndex")).toInt(0);
        m_tabBar->setCurrentIndex(qBound(0, currentIndex, m_tabBar->count() - 1));
    }

    return true;
}

void MainWindow::saveWindowPlacement() const
{
    QRect savedGeometry = isMaximized() ? normalGeometry() : geometry();
    if (!savedGeometry.isValid())
        savedGeometry = geometry();

    if (!savedGeometry.isValid())
        return;

    QJsonObject root {
        { QStringLiteral("x"), savedGeometry.x() },
        { QStringLiteral("y"), savedGeometry.y() },
        { QStringLiteral("width"), savedGeometry.width() },
        { QStringLiteral("height"), savedGeometry.height() },
        { QStringLiteral("maximized"), isMaximized() },
    };

    QFile file(windowPlacementPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void MainWindow::restoreWindowPlacement()
{
    QFile file(windowPlacementPath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    const QRect savedRect(
        root.value(QStringLiteral("x")).toInt(x()),
        root.value(QStringLiteral("y")).toInt(y()),
        root.value(QStringLiteral("width")).toInt(width()),
        root.value(QStringLiteral("height")).toInt(height()));

    if (!savedRect.isValid() || savedRect.width() < 640 || savedRect.height() < 480)
        return;

    QRect boundedRect = savedRect;
    const QList<QScreen *> screens = QGuiApplication::screens();
    bool visibleOnAnyScreen = false;
    for (QScreen *screen : screens) {
        if (!screen)
            continue;

        const QRect available = screen->availableGeometry();
        if (available.intersects(boundedRect.adjusted(32, 32, -32, -32))) {
            visibleOnAnyScreen = true;
            boundedRect.moveLeft(qMax(available.left(), boundedRect.left()));
            boundedRect.moveTop(qMax(available.top(), boundedRect.top()));
            boundedRect.setWidth(qMin(boundedRect.width(), available.width()));
            boundedRect.setHeight(qMin(boundedRect.height(), available.height()));
            break;
        }
    }

    if (!visibleOnAnyScreen) {
        if (QScreen *primaryScreen = QGuiApplication::primaryScreen()) {
            const QRect available = primaryScreen->availableGeometry();
            boundedRect.setWidth(qMin(boundedRect.width(), available.width()));
            boundedRect.setHeight(qMin(boundedRect.height(), available.height()));
            boundedRect.moveCenter(available.center());
        }
    }

    m_restoringWindowPlacement = true;
    setGeometry(boundedRect);

    if (root.value(QStringLiteral("maximized")).toBool()) {
        QTimer::singleShot(0, this, [this]() {
            showMaximized();
            refreshIcons();
            m_restoringWindowPlacement = false;
        });
        return;
    }

    refreshIcons();
    m_restoringWindowPlacement = false;
}

QUrl MainWindow::searchUrlForQuery(const QString &query) const
{
    QString baseUrl;
    QString queryKey = QStringLiteral("q");

    const QString engine = m_settings->searchEngine();
    if (engine == QLatin1String("google")) {
        baseUrl = QStringLiteral("https://www.google.com/search");
    } else if (engine == QLatin1String("bing")) {
        baseUrl = QStringLiteral("https://www.bing.com/search");
    } else if (engine == QLatin1String("brave")) {
        baseUrl = QStringLiteral("https://search.brave.com/search");
    } else if (engine == QLatin1String("startpage")) {
        baseUrl = QStringLiteral("https://www.startpage.com/sp/search");
        queryKey = QStringLiteral("query");
    } else {
        baseUrl = QStringLiteral("https://duckduckgo.com/");
    }

    QUrl url(baseUrl);
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(queryKey, query);
    url.setQuery(urlQuery);
    return url;
}

QUrl MainWindow::resolveInternalUrl(const QString &page) const
{
    static const QHash<QString, QString> internalPages = {
        { QStringLiteral("codec-test"), QStringLiteral("qrc:/pages/codec-test.html") },
        { QStringLiteral("newtab"),     QStringLiteral("qrc:/pages/newtab.html") },
        { QStringLiteral("settings"),   QStringLiteral("qrc:/pages/settings.html") },
    };
    auto it = internalPages.find(page);
    if (it != internalPages.end())
        return QUrl(*it);
    return {};
}

QUrl MainWindow::normalizedYouTubeUrl(const QUrl &url) const
{
    if (!m_settings || !m_settings->value(QStringLiteral("content.youtubeShortsAsNormalVideos")).toBool())
        return url;

    if (!url.isValid())
        return url;

    const QString host = url.host().toLower();
    if (host != QLatin1String("youtube.com")
        && host != QLatin1String("www.youtube.com")
        && host != QLatin1String("m.youtube.com")) {
        return url;
    }

    const QString path = url.path();
    if (!path.startsWith(QLatin1String("/shorts/")))
        return url;

    const QStringList segments = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.size() < 2 || segments.at(0) != QLatin1String("shorts"))
        return url;

    const QString videoId = segments.at(1).trimmed();
    if (videoId.isEmpty())
        return url;

    QUrl normalized(url);
    normalized.setPath(QStringLiteral("/watch"));

    QUrlQuery query(normalized);
    query.removeAllQueryItems(QStringLiteral("v"));
    query.addQueryItem(QStringLiteral("v"), videoId);
    normalized.setQuery(query);
    return normalized;
}

QString MainWindow::siteSettingValue(const QString &path, const QString &fallback) const
{
    if (!m_settings)
        return fallback;

    const QString value = m_settings->value(path).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

QString MainWindow::permissionPolicyForOrigin(const QString &permissionType, const QUrl &origin, const QString &defaultPolicy) const
{
    if (!m_settings)
        return defaultPolicy;

    const QString perSitePolicy = m_settings->sitePermissionRule(permissionType, origin).trimmed().toLower();
    if (!perSitePolicy.isEmpty())
        return perSitePolicy;

    return defaultPolicy;
}

QWebEnginePage::PermissionPolicy MainWindow::promptForPermissionDecision(const QUrl &origin,
                                                                         const QStringList &permissionTypes,
                                                                         bool *rememberChoice)
{
    if (rememberChoice)
        *rememberChoice = false;

    QMessageBox prompt(this);
    prompt.setWindowTitle(QStringLiteral("Site Permission Request"));
    prompt.setIcon(QMessageBox::Question);
    prompt.setText(QStringLiteral("%1 wants access to %2.")
        .arg(origin.host().isEmpty() ? origin.toString() : origin.host(), joinPermissionLabels(permissionTypes)));
    prompt.setInformativeText(QStringLiteral("Choose whether to allow or block this request."));

    QPushButton *allowButton = prompt.addButton(QStringLiteral("Allow"), QMessageBox::AcceptRole);
    QPushButton *blockButton = prompt.addButton(QStringLiteral("Block"), QMessageBox::DestructiveRole);
    QPushButton *askButton = prompt.addButton(QStringLiteral("Ask Later"), QMessageBox::RejectRole);

    QCheckBox rememberCheckBox(QStringLiteral("Remember this decision for this site"), &prompt);
    prompt.setCheckBox(&rememberCheckBox);
    prompt.exec();

    if (rememberChoice)
        *rememberChoice = rememberCheckBox.isChecked();

    if (qobject_cast<QPushButton *>(prompt.clickedButton()) == allowButton)
        return QWebEnginePage::PermissionGrantedByUser;
    if (qobject_cast<QPushButton *>(prompt.clickedButton()) == blockButton)
        return QWebEnginePage::PermissionDeniedByUser;
    Q_UNUSED(askButton);
    return QWebEnginePage::PermissionUnknown;
}

void MainWindow::applyFeaturePermission(QWebEnginePage *page, const QUrl &origin, QWebEnginePage::Feature feature)
{
    if (!page)
        return;

    switch (feature) {
    case QWebEnginePage::Notifications: {
        const QString policy = permissionPolicyForOrigin(
            QStringLiteral("notifications"),
            origin,
            siteSettingValue(QStringLiteral("content.siteSettings.notifications"), QStringLiteral("ask")));
        QWebEnginePage::PermissionPolicy permission = policyForValue(policy);
        if (permission == QWebEnginePage::PermissionUnknown) {
            m_settings->rememberSitePermissionRequest(QStringLiteral("notifications"), origin);
            bool rememberChoice = false;
            permission = promptForPermissionDecision(origin, { QStringLiteral("notifications") }, &rememberChoice);
            if (rememberChoice && permission != QWebEnginePage::PermissionUnknown) {
                m_settings->upsertSitePermissionRule(
                    QStringLiteral("notifications"),
                    origin.toString(),
                    permission == QWebEnginePage::PermissionGrantedByUser ? QStringLiteral("allow") : QStringLiteral("block"));
            }
        }
        page->setFeaturePermission(origin, feature, permission);
        break;
    }
    case QWebEnginePage::Geolocation: {
        const QString policy = permissionPolicyForOrigin(
            QStringLiteral("location"),
            origin,
            siteSettingValue(QStringLiteral("content.siteSettings.location"), QStringLiteral("ask")));
        QWebEnginePage::PermissionPolicy permission = policyForValue(policy);
        if (permission == QWebEnginePage::PermissionUnknown) {
            m_settings->rememberSitePermissionRequest(QStringLiteral("location"), origin);
            bool rememberChoice = false;
            permission = promptForPermissionDecision(origin, { QStringLiteral("location") }, &rememberChoice);
            if (rememberChoice && permission != QWebEnginePage::PermissionUnknown) {
                m_settings->upsertSitePermissionRule(
                    QStringLiteral("location"),
                    origin.toString(),
                    permission == QWebEnginePage::PermissionGrantedByUser ? QStringLiteral("allow") : QStringLiteral("block"));
            }
        }
        page->setFeaturePermission(origin, feature, permission);
        break;
    }
    case QWebEnginePage::MediaAudioCapture: {
        const QString policy = permissionPolicyForOrigin(
            QStringLiteral("microphone"),
            origin,
            siteSettingValue(QStringLiteral("content.siteSettings.microphone"), QStringLiteral("ask")));
        QWebEnginePage::PermissionPolicy permission = policyForValue(policy);
        if (permission == QWebEnginePage::PermissionUnknown) {
            m_settings->rememberSitePermissionRequest(QStringLiteral("microphone"), origin);
            bool rememberChoice = false;
            permission = promptForPermissionDecision(origin, { QStringLiteral("microphone") }, &rememberChoice);
            if (rememberChoice && permission != QWebEnginePage::PermissionUnknown) {
                m_settings->upsertSitePermissionRule(
                    QStringLiteral("microphone"),
                    origin.toString(),
                    permission == QWebEnginePage::PermissionGrantedByUser ? QStringLiteral("allow") : QStringLiteral("block"));
            }
        }
        page->setFeaturePermission(origin, feature, permission);
        break;
    }
    case QWebEnginePage::MediaVideoCapture: {
        const QString policy = permissionPolicyForOrigin(
            QStringLiteral("camera"),
            origin,
            siteSettingValue(QStringLiteral("content.siteSettings.camera"), QStringLiteral("ask")));
        QWebEnginePage::PermissionPolicy permission = policyForValue(policy);
        if (permission == QWebEnginePage::PermissionUnknown) {
            m_settings->rememberSitePermissionRequest(QStringLiteral("camera"), origin);
            bool rememberChoice = false;
            permission = promptForPermissionDecision(origin, { QStringLiteral("camera") }, &rememberChoice);
            if (rememberChoice && permission != QWebEnginePage::PermissionUnknown) {
                m_settings->upsertSitePermissionRule(
                    QStringLiteral("camera"),
                    origin.toString(),
                    permission == QWebEnginePage::PermissionGrantedByUser ? QStringLiteral("allow") : QStringLiteral("block"));
            }
        }
        page->setFeaturePermission(origin, feature, permission);
        break;
    }
    case QWebEnginePage::MediaAudioVideoCapture: {
        const QString cameraPolicy = permissionPolicyForOrigin(
            QStringLiteral("camera"),
            origin,
            siteSettingValue(QStringLiteral("content.siteSettings.camera"), QStringLiteral("ask")));
        const QString microphonePolicy = permissionPolicyForOrigin(
            QStringLiteral("microphone"),
            origin,
            siteSettingValue(QStringLiteral("content.siteSettings.microphone"), QStringLiteral("ask")));

        QWebEnginePage::PermissionPolicy permission = QWebEnginePage::PermissionUnknown;
        if (cameraPolicy == QLatin1String("block") || microphonePolicy == QLatin1String("block")) {
            permission = QWebEnginePage::PermissionDeniedByUser;
        } else if (cameraPolicy == QLatin1String("allow") && microphonePolicy == QLatin1String("allow")) {
            permission = QWebEnginePage::PermissionGrantedByUser;
        } else {
            m_settings->rememberSitePermissionRequest(QStringLiteral("camera"), origin);
            m_settings->rememberSitePermissionRequest(QStringLiteral("microphone"), origin);
            bool rememberChoice = false;
            permission = promptForPermissionDecision(origin,
                { QStringLiteral("camera"), QStringLiteral("microphone") },
                &rememberChoice);
            if (rememberChoice && permission != QWebEnginePage::PermissionUnknown) {
                const QString storedPolicy = permission == QWebEnginePage::PermissionGrantedByUser
                    ? QStringLiteral("allow")
                    : QStringLiteral("block");
                m_settings->upsertSitePermissionRule(QStringLiteral("camera"), origin.toString(), storedPolicy);
                m_settings->upsertSitePermissionRule(QStringLiteral("microphone"), origin.toString(), storedPolicy);
            }
        }

        page->setFeaturePermission(origin, feature, permission);
        break;
    }
    default:
        page->setFeaturePermission(origin, feature, QWebEnginePage::PermissionUnknown);
        break;
    }
}

bool MainWindow::isSettingsUrl(const QUrl &url) const
{
    return url.scheme() == QLatin1String("qrc")
        && url.path() == QLatin1String("/pages/settings.html");
}

bool MainWindow::looksLikeUrl(const QString &input) const
{
    if (input.contains(QLatin1Char(' ')))
        return false;

    if (input.startsWith(QLatin1String("localhost")) || input.startsWith(QLatin1String("127.")))
        return true;

    static const QRegularExpression hostPattern(QStringLiteral(R"(^[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)+([/:?#].*)?$)"));
    return hostPattern.match(input).hasMatch();
}

void MainWindow::handleDownloadRequested(QWebEngineDownloadRequest *download)
{
    if (!download)
        return;

    QString targetDirectory = m_settings->value(QStringLiteral("downloads.defaultPath")).toString();
    if (targetDirectory.isEmpty()) {
        targetDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        if (targetDirectory.isEmpty())
            targetDirectory = QDir::homePath();
    }

    QString targetFile = download->suggestedFileName();
    if (targetFile.isEmpty())
        targetFile = QFileInfo(download->downloadFileName()).fileName();
    if (targetFile.isEmpty())
        targetFile = QStringLiteral("download.bin");

    if (m_settings->value(QStringLiteral("downloads.askWhereToSave")).toBool()) {
        const QString selectedFile = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save Download"),
            QDir(targetDirectory).filePath(targetFile));

        if (selectedFile.isEmpty()) {
            download->cancel();
            return;
        }

        QFileInfo selectedInfo(selectedFile);
        download->setDownloadDirectory(selectedInfo.absolutePath());
        download->setDownloadFileName(selectedInfo.fileName());
    } else {
        download->setDownloadDirectory(targetDirectory);
        download->setDownloadFileName(targetFile);
    }

    download->accept();
}

void MainWindow::clearBrowsingDataIfNeeded()
{
    if (!m_profile || !m_settings->value(QStringLiteral("privacy.clearDataOnExit")).toBool())
        return;

    m_profile->cookieStore()->deleteAllCookies();
    m_profile->clearHttpCache();
    m_profile->clearAllVisitedLinks();
    if (m_history)
        m_history->clear();
}

Qt::Edges MainWindow::resizeEdgesForGlobalPos(const QPoint &globalPos) const
{
    if (!windowHandle())
        return Qt::Edges();

    constexpr int resizeMargin = 6;
    const QRect frame = frameGeometry();
    Qt::Edges edges;

    if (globalPos.x() >= frame.left() && globalPos.x() <= frame.left() + resizeMargin)
        edges |= Qt::LeftEdge;
    else if (globalPos.x() <= frame.right() && globalPos.x() >= frame.right() - resizeMargin)
        edges |= Qt::RightEdge;

    if (globalPos.y() >= frame.top() && globalPos.y() <= frame.top() + resizeMargin)
        edges |= Qt::TopEdge;
    else if (globalPos.y() <= frame.bottom() && globalPos.y() >= frame.bottom() - resizeMargin)
        edges |= Qt::BottomEdge;

    return edges;
}

void MainWindow::updateResizeCursor(const QPoint &globalPos)
{
    if (m_dragging || m_trackingResize)
        return;

    const Qt::Edges edges = resizeEdgesForGlobalPos(globalPos);
    Qt::CursorShape shape = Qt::ArrowCursor;

    if (edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge))
        shape = Qt::SizeFDiagCursor;
    else if (edges == (Qt::TopEdge | Qt::RightEdge) || edges == (Qt::BottomEdge | Qt::LeftEdge))
        shape = Qt::SizeBDiagCursor;
    else if (edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge))
        shape = Qt::SizeHorCursor;
    else if (edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge))
        shape = Qt::SizeVerCursor;

    setCursor(shape);
}

QString MainWindow::iconPath(const QString &name) const
{
    const QString theme = m_darkMode ? QStringLiteral("dark") : QStringLiteral("light");
    return QStringLiteral(":/icons/%1/%2.svg").arg(theme, name);
}
