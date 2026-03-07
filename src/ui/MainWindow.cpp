#include "MainWindow.h"

#include "browser/GhostRequestInterceptor.h"
#include "core/CookieManager.h"
#include "core/HistoryManager.h"
#include "core/SettingsManager.h"

#include <QAction>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
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

// ── Construction ──────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    resize(1280, 800);
    m_settings = new SettingsManager(this);
    m_history  = new HistoryManager(this);
    m_profile = new QWebEngineProfile(QStringLiteral("Ghost"), this);
    m_requestInterceptor = new GhostRequestInterceptor(m_settings, this);
    connect(m_settings, &SettingsManager::settingsChanged,
            m_requestInterceptor, &GhostRequestInterceptor::refreshSettings);
    configureProfile();
    m_profile->setUrlRequestInterceptor(m_requestInterceptor);
    m_cookies = new CookieManager(m_profile->cookieStore(), this);
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

    root->addWidget(m_titleBar);
    root->addWidget(m_navBar);
    root->addWidget(m_bookmarksBar);
    root->addWidget(m_pageStack, 1);

    setCentralWidget(central);
    trackMouseForResize(this);
    trackMouseForResize(central);
    trackMouseForResize(m_titleBar);
    trackMouseForResize(m_tabBar);
    trackMouseForResize(m_navBar);
    trackMouseForResize(m_bookmarksBar);
    trackMouseForResize(m_urlBar);
    trackMouseForResize(m_pageStack);

    connect(m_settings, &SettingsManager::settingsChanged, this, [this](const QString &) {
        applyAppearanceSettings();
        applyContentSettings();
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
    applyPrivacySettings();
    applyDownloadSettings();
    applySystemSettings();
    applyStyles();

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

// ── Content ───────────────────────────────────────────────────

void MainWindow::buildContentArea()
{
    m_pageStack = new QStackedWidget(this);
    m_pageStack->setObjectName("pageStack");
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
}

// ── Styles ────────────────────────────────────────────────────

void MainWindow::applyStyles()
{
    const QString background = m_darkMode ? QStringLiteral("#1E1E2E") : QStringLiteral("#F3F5F9");
    const QString surface = m_darkMode ? QStringLiteral("#2A2A3C") : QStringLiteral("#FFFFFF");
    const QString surfaceHover = m_darkMode ? QStringLiteral("#333346") : QStringLiteral("#E8ECF5");
    const QString textPrimary = m_darkMode ? QStringLiteral("#E0E0E0") : QStringLiteral("#1F2430");
    const QString textMuted = m_darkMode ? QStringLiteral("#AAA") : QStringLiteral("#5E6573");
    const QString border = m_darkMode ? QStringLiteral("#3A3A4A") : QStringLiteral("#D9DFEA");
    const QString accent = QStringLiteral("#5B5FC7");

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
    )").arg(background, surface, textMuted, textPrimary, accent, surfaceHover, iconPath(QStringLiteral("x")), border));
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

    for (int i = 0; i < m_pageStack->count(); ++i) {
        if (auto *view = qobject_cast<QWebEngineView *>(m_pageStack->widget(i))) {
            applyViewSettings(view);
            refreshTabPresentation(view);
        }
    }
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
    const QColor backgroundColor = m_darkMode ? QColor(0x1E, 0x1E, 0x2E) : QColor(0xF3, 0xF5, 0xF9);

    view->settings()->setFontSize(QWebEngineSettings::DefaultFontSize, fontSize > 0 ? fontSize : 16);
    view->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, !autoplayEnabled);
    view->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, fullScreenEnabled);
    view->settings()->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
    view->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    view->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, javascriptEnabled);
    view->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, popupsEnabled);
    const bool hwAccel = m_settings->value(QStringLiteral("system.hardwareAcceleration")).toBool();
    view->settings()->setAttribute(QWebEngineSettings::WebGLEnabled, hwAccel);
    view->settings()->setAttribute(QWebEngineSettings::Accelerated2dCanvasEnabled, hwAccel);
    view->setZoomFactor((zoomLevel > 0 ? zoomLevel : 100) / 100.0);
    view->page()->setBackgroundColor(backgroundColor);
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
}

void MainWindow::onClose()
{
    close();
}

// ── Drag handling (title bar) ────────────────────────────────

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSessionState();
    clearBrowsingDataIfNeeded();
    QMainWindow::closeEvent(event);
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

    if (isSettingsUrl(url))
        attachSettingsBridge(view);

    view->setUrl(normalizedYouTubeUrl(url));

    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &url) {
        const QUrl normalized = normalizedYouTubeUrl(url);
        if (normalized != url) {
            view->setUrl(normalized);
            return;
        }

        refreshTabPresentation(view);
        updateUrlBar(url);
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
    connect(view, &QWebEngineView::loadFinished, this, [this, view](bool ok) {
        if (ok && view) {
            refreshTabPresentation(view);
            m_history->recordVisit(view->url(), view->title());
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

void MainWindow::applyFeaturePermission(QWebEnginePage *page, const QUrl &origin, QWebEnginePage::Feature feature)
{
    if (!page)
        return;

    auto policyFor = [](const QString &value) {
        if (value == QLatin1String("allow"))
            return QWebEnginePage::PermissionGrantedByUser;
        if (value == QLatin1String("block"))
            return QWebEnginePage::PermissionDeniedByUser;
        return QWebEnginePage::PermissionUnknown;
    };

    switch (feature) {
    case QWebEnginePage::Notifications:
        page->setFeaturePermission(origin, feature,
            policyFor(siteSettingValue(QStringLiteral("content.siteSettings.notifications"), QStringLiteral("ask"))));
        break;
    case QWebEnginePage::Geolocation:
        page->setFeaturePermission(origin, feature,
            policyFor(siteSettingValue(QStringLiteral("content.siteSettings.location"), QStringLiteral("ask"))));
        break;
    case QWebEnginePage::MediaAudioCapture:
        page->setFeaturePermission(origin, feature,
            policyFor(siteSettingValue(QStringLiteral("content.siteSettings.microphone"), QStringLiteral("ask"))));
        break;
    case QWebEnginePage::MediaVideoCapture:
        page->setFeaturePermission(origin, feature,
            policyFor(siteSettingValue(QStringLiteral("content.siteSettings.camera"), QStringLiteral("ask"))));
        break;
    case QWebEnginePage::MediaAudioVideoCapture: {
        const QString cameraPolicy = siteSettingValue(QStringLiteral("content.siteSettings.camera"), QStringLiteral("ask"));
        const QString microphonePolicy = siteSettingValue(QStringLiteral("content.siteSettings.microphone"), QStringLiteral("ask"));
        const QString combinedPolicy = (cameraPolicy == QLatin1String("block") || microphonePolicy == QLatin1String("block"))
            ? QStringLiteral("block")
            : (cameraPolicy == QLatin1String("allow") && microphonePolicy == QLatin1String("allow"))
                ? QStringLiteral("allow")
                : QStringLiteral("ask");
        page->setFeaturePermission(origin, feature, policyFor(combinedPolicy));
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
