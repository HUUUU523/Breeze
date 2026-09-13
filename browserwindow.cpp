#include "adblocker.h"
#include "aidialog.h"
#include "bookmarkmanager.h"
#include "browserwindow.h"
#include "aisidebar.h"
#include "bookmarksidebar.h"
#include "cookiemanagerdialog.h"
#include "downloadmanager.h"
#include "extension.h"
#include "extensiondialog.h"
#include "historymanager.h"
#include "settingsdialog.h"
#include "syncmerge.h"
#include "syncdialog.h"
#include "toolbox.h"
#include "translator.h"
#include "updatemanager.h"
#include "userscriptmanager.h"
#include "webview.h"

#include <QAction>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QShortcut>
#include <QApplication>
#include <QCloseEvent>
#include <QCompleter>
#include <QStringListModel>
#include <QDate>
#include <QDateTime>
#include <QActionGroup>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QTimer>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QLabel>
#include <QHash>
#include <QSet>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QTextStream>
#include <QProgressBar>
#include <QWebEnginePage>
#include <QWebEngineFindTextResult>
#include <QTabBar>
#include <QMouseEvent>
#include <QEvent>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QWebEngineDownloadRequest>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QStatusBar>

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
{
    migrateLegacyData();

    m_adBlocker = new AdBlocker(this);

    setupUi();
    setupActions();
    setupBookmarks();
    setupDownloads();
    setupHistory();

    // 启动时恢复上次会话；若无会话则打开主页
    restoreSession();

    resize(1280, 800);
    setWindowTitle(QStringLiteral("Breeze ") + QStringLiteral(BREEZE_VERSION));

    applyTheme();
}

BrowserWindow::~BrowserWindow() = default;

void BrowserWindow::migrateLegacyData()
{
    // 旧版数据写在程序目录；新版写在 AppData。
    // 仅当旧文件存在、且新文件不存在时才迁移，避免覆盖新数据。
    const QString oldDir = QApplication::applicationDirPath();
    const QString newDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (oldDir.isEmpty() || newDir.isEmpty() || oldDir == newDir)
        return;

    const QStringList files = {
        QStringLiteral("bookmarks.json"),
        QStringLiteral("history.json"),
    };

    QDir().mkpath(newDir);
    for (const QString &name : files) {
        const QString oldPath = oldDir + QLatin1Char('/') + name;
        const QString newPath = newDir + QLatin1Char('/') + name;
        if (QFile::exists(oldPath) && !QFile::exists(newPath)) {
            QFile::copy(oldPath, newPath);
        }
    }
}


void BrowserWindow::closeEvent(QCloseEvent *event)
{
    saveBookmarks();
    saveHistory();
    saveSession();
    QMainWindow::closeEvent(event);
}

bool BrowserWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_shieldLabel && event->type() == QEvent::MouseButtonRelease) {
        showBlockedDetails();
        return true;
    }
    if (obj == m_tabs->tabBar() && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton) {
            const int idx = m_tabs->tabBar()->tabAt(me->position().toPoint());
            if (idx >= 0)
                onCloseTab(idx);
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void BrowserWindow::setupUi()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setExpanding(false);
    // 标签过多时可滚动 + 省略号
    m_tabs->tabBar()->setUsesScrollButtons(true);
    m_tabs->setElideMode(Qt::ElideRight);
    m_tabs->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::currentChanged, this, &BrowserWindow::onTabChanged);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &BrowserWindow::onCloseTab);

    // 中键点击标签 → 关闭（与主流浏览器一致）
    m_tabs->tabBar()->installEventFilter(this);

    // 右键标签 → 静音等
    connect(m_tabs->tabBar(), &QTabBar::customContextMenuRequested,
            this, &BrowserWindow::onTabBarContextMenu);

    m_progress = new QProgressBar(this);
    m_progress->setMaximumWidth(160);
    m_progress->setMaximumHeight(14);
    m_progress->setTextVisible(false);
    m_progress->setRange(0, 100);
    m_progress->hide();
    statusBar()->addPermanentWidget(m_progress);
}

void BrowserWindow::setupActions()
{
    QToolBar *navBar = addToolBar(QStringLiteral("Navigation"));
    navBar->setMovable(false);
    navBar->setIconSize(QSize(18, 18));

    m_actBack    = navBar->addAction(style()->standardIcon(QStyle::SP_ArrowBack),   QString());
    m_actForward = navBar->addAction(style()->standardIcon(QStyle::SP_ArrowForward), QString());
    m_actReload  = navBar->addAction(style()->standardIcon(QStyle::SP_BrowserReload), QString());
    // 极简：不再显示停止/主页按钮

    m_actBack->setToolTip(TR(QStringLiteral("nav.back")));
    m_actForward->setToolTip(TR(QStringLiteral("nav.forward")));
    m_actReload->setToolTip(TR(QStringLiteral("nav.reload")));

    navBar->addSeparator();

    // 安全状态指示（锁图标）
    m_securityLabel = new QLabel(this);
    m_securityLabel->setToolTip(QStringLiteral("连接安全性"));
    m_securityLabel->setMinimumWidth(20);
    navBar->addWidget(m_securityLabel);

    // 地址栏
    m_urlBar = new QLineEdit(this);
    m_urlBar->setClearButtonEnabled(true);
    m_urlBar->setPlaceholderText(QStringLiteral("搜索或输入网址"));
    m_urlBar->setMinimumWidth(400);
    navBar->addWidget(m_urlBar);

    // 地址栏智能补全（按需重建 model，避免维护多个刷新点）
    m_completer = new QCompleter(this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_urlBar->setCompleter(m_completer);
    connect(m_urlBar, &QLineEdit::textEdited, this, &BrowserWindow::refreshUrlCompleter);

    // 盾牌：显示当前站点拦截数
    m_shieldLabel = new QLabel(this);
    m_shieldLabel->setToolTip(QStringLiteral("本页拦截的广告/追踪请求"));
    m_shieldLabel->setCursor(Qt::PointingHandCursor);
    m_shieldLabel->setMinimumWidth(48);
    m_shieldLabel->setText(QStringLiteral("🛡 0"));
    m_shieldLabel->installEventFilter(this);
    navBar->addWidget(m_shieldLabel);

    navBar->addSeparator();
    m_actNewTab = navBar->addAction(style()->standardIcon(QStyle::SP_FileDialogNewFolder), QString());
    m_actNewTab->setToolTip(TR(QStringLiteral("nav.newTab")));

    // ---- 右侧 ⋮ 主菜单 ----
    auto *menuBtn = new QToolButton(navBar);
    menuBtn->setText(QStringLiteral("⋮"));
    menuBtn->setToolTip(QStringLiteral("菜单"));
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setAutoRaise(true);
    auto *mainMenu = new QMenu(menuBtn);

    m_actNewPrivateTab = mainMenu->addAction(QStringLiteral("新建隐私标签页"));
    m_actNewPrivateTab->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")));
    mainMenu->addSeparator();

    m_actBookmark = mainMenu->addAction(QStringLiteral("添加书签"));
    m_actBookmark->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    m_actManageBookmarks = mainMenu->addAction(QStringLiteral("管理书签…"));
    QAction *actImportBm = mainMenu->addAction(QStringLiteral("导入书签（HTML）…"));
    QAction *actExportBm = mainMenu->addAction(QStringLiteral("导出书签（HTML）…"));
    m_actShowBookmarkBar = mainMenu->addAction(QStringLiteral("显示书签栏"));
    m_actShowBookmarkBar->setCheckable(true);
    m_actShowBookmarkBar->setChecked(false);

    m_actHistory   = mainMenu->addAction(QStringLiteral("历史记录…"));
    QAction *actImportHis = mainMenu->addAction(QStringLiteral("导入历史（JSON）…"));
    QAction *actExportHis = mainMenu->addAction(QStringLiteral("导出历史（JSON）…"));
    m_actDownloads = mainMenu->addAction(QStringLiteral("下载…"));
    mainMenu->addSeparator();

    QAction *actFind = mainMenu->addAction(QStringLiteral("查找…"));
    actFind->setShortcut(QKeySequence::Find);
    QAction *actPrint = mainMenu->addAction(QStringLiteral("打印…"));
    actPrint->setShortcut(QKeySequence::Print);
    QAction *actPdf = mainMenu->addAction(QStringLiteral("保存为 PDF…"));
    QAction *actSaveHtml = mainMenu->addAction(QStringLiteral("保存为 MHTML…"));
    QAction *actCapture = mainMenu->addAction(QStringLiteral("截图当前页…"));
    QAction *actCaptureFull = mainMenu->addAction(QStringLiteral("整页截图…"));
    mainMenu->addSeparator();

    QMenu *themeMenu = mainMenu->addMenu(QStringLiteral("主题"));
    auto *themeGroup = new QActionGroup(themeMenu);
    themeGroup->setExclusive(true);
    m_actThemeSystem = themeMenu->addAction(QStringLiteral("跟随系统"));
    m_actThemeLight  = themeMenu->addAction(QStringLiteral("浅色"));
    m_actThemeDark   = themeMenu->addAction(QStringLiteral("深色"));
    for (QAction *a : {m_actThemeSystem, m_actThemeLight, m_actThemeDark}) {
        a->setCheckable(true);
        themeGroup->addAction(a);
    }

    QMenu *langMenu = mainMenu->addMenu(QStringLiteral("语言"));
    langMenu->addAction(QStringLiteral("中文"), this,
        [this]{ Translator::instance()->setLanguage(QStringLiteral("zh")); });
    langMenu->addAction(QStringLiteral("English"), this,
        [this]{ Translator::instance()->setLanguage(QStringLiteral("en")); });
    mainMenu->addSeparator();

    QAction *actAiChat = mainMenu->addAction(QStringLiteral("AI 对话…"));
    QAction *actAiSummary = mainMenu->addAction(QStringLiteral("AI 总结当前页"));
    QAction *actAiTranslate = mainMenu->addAction(QStringLiteral("AI 翻译当前页"));
    QAction *actAiSidebar = mainMenu->addAction(QStringLiteral("AI 侧边栏"));
    QAction *actBmSidebar = mainMenu->addAction(QStringLiteral("书签/历史侧边栏"));
    QAction *actPageQa = mainMenu->addAction(QStringLiteral("网页问答…"));
    mainMenu->addSeparator();

    QAction *actAdBlock = mainMenu->addAction(QStringLiteral("拦截广告"));
    actAdBlock->setCheckable(true);
    actAdBlock->setChecked(m_adBlocker->isEnabled());
    connect(actAdBlock, &QAction::toggled, this, &BrowserWindow::toggleAdBlock);

    QAction *actUs = mainMenu->addAction(QStringLiteral("用户脚本…"));
    QAction *actExt = mainMenu->addAction(QStringLiteral("扩展管理…"));
    QAction *actToolbox = mainMenu->addAction(QStringLiteral("工具箱…"));
    QAction *actSync = mainMenu->addAction(QStringLiteral("云同步…"));
    mainMenu->addSeparator();

    QAction *actSaveSession = mainMenu->addAction(QStringLiteral("保存当前会话…"));
    QAction *actManageSession = mainMenu->addAction(QStringLiteral("会话管理…"));
    QAction *actCheckUpdate = mainMenu->addAction(QStringLiteral("检查更新…"));
    QAction *actClearData = mainMenu->addAction(QStringLiteral("清除浏览数据…"));
    QAction *actCookies = mainMenu->addAction(QStringLiteral("Cookie 管理…"));
    m_actSettings = mainMenu->addAction(QStringLiteral("设置…"));
    QAction *actQuit = mainMenu->addAction(QStringLiteral("退出"));
    actQuit->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));

    menuBtn->setMenu(mainMenu);
    navBar->addWidget(menuBtn);

    // ---- 连接 ----
    connect(m_actBack,    &QAction::triggered, this, &BrowserWindow::navBack);
    connect(m_actForward, &QAction::triggered, this, &BrowserWindow::navForward);
    connect(m_actReload,  &QAction::triggered, this, &BrowserWindow::navReload);
    connect(m_actNewTab,  &QAction::triggered, this, &BrowserWindow::onNewTab);
    connect(m_actNewPrivateTab, &QAction::triggered, this, &BrowserWindow::onNewPrivateTab);
    connect(m_actBookmark, &QAction::triggered, this, &BrowserWindow::addBookmarkForCurrentPage);
    connect(m_actManageBookmarks, &QAction::triggered, this, &BrowserWindow::showBookmarkManager);
    connect(actImportBm, &QAction::triggered, this, &BrowserWindow::importBookmarks);
    connect(actExportBm, &QAction::triggered, this, &BrowserWindow::exportBookmarks);
    connect(m_actShowBookmarkBar, &QAction::toggled, this, [this](bool on){
        if (m_bookmarkBar) m_bookmarkBar->setVisible(on);
    });
    connect(m_actHistory, &QAction::triggered, this, &BrowserWindow::showHistory);
    connect(actImportHis, &QAction::triggered, this, &BrowserWindow::importHistory);
    connect(actExportHis, &QAction::triggered, this, &BrowserWindow::exportHistory);
    connect(m_actDownloads, &QAction::triggered, this, &BrowserWindow::showDownloads);
    connect(actFind, &QAction::triggered, this, &BrowserWindow::showFindBar);
    connect(actPrint, &QAction::triggered, this, &BrowserWindow::printPage);
    connect(actPdf, &QAction::triggered, this, &BrowserWindow::savePageAsPdf);
    connect(actSaveHtml, &QAction::triggered, this, &BrowserWindow::savePageAsHtml);
    connect(actCapture, &QAction::triggered, this, &BrowserWindow::capturePage);
    connect(actCaptureFull, &QAction::triggered, this, &BrowserWindow::captureFullPage);
    connect(m_actThemeSystem, &QAction::triggered, this, [this]{ setThemeMode(QStringLiteral("system")); });
    connect(m_actThemeLight,  &QAction::triggered, this, [this]{ setThemeMode(QStringLiteral("light")); });
    connect(m_actThemeDark,   &QAction::triggered, this, [this]{ setThemeMode(QStringLiteral("dark")); });
    connect(actAiChat, &QAction::triggered, this, &BrowserWindow::showAiChat);
    connect(actAiSummary, &QAction::triggered, this, &BrowserWindow::aiSummarizePage);
    connect(actAiTranslate, &QAction::triggered, this, &BrowserWindow::aiTranslatePage);
    connect(actAiSidebar, &QAction::triggered, this, &BrowserWindow::toggleAiSidebar);
    connect(actBmSidebar, &QAction::triggered, this, &BrowserWindow::toggleBookmarkSidebar);
    connect(actPageQa, &QAction::triggered, this, [this]() {
        if (!m_aiSidebar) {
            m_aiSidebar = new AiSidebar(this);
            addDockWidget(Qt::RightDockWidgetArea, m_aiSidebar);
        }
        m_aiSidebar->show();
        auto *v = currentView();
        if (v) {
            v->page()->toPlainText([this, v](const QString &text) {
                if (m_aiSidebar)
                    m_aiSidebar->setPageContext(v->title(), text.left(8000));
            });
        }
    });
    connect(actUs, &QAction::triggered, this, &BrowserWindow::showUserScriptManager);
    connect(actExt, &QAction::triggered, this, [this]() {
        ExtensionDialog dlg(this);
        dlg.exec();
    });
    connect(actToolbox, &QAction::triggered, this, &BrowserWindow::showToolbox);
    connect(actSync, &QAction::triggered, this, &BrowserWindow::showSyncDialog);
    connect(actCheckUpdate, &QAction::triggered, this, &BrowserWindow::checkForUpdates);
    connect(actClearData, &QAction::triggered, this, &BrowserWindow::clearBrowsingData);
    connect(actCookies, &QAction::triggered, this, [this]() {
        CookieManagerDialog dlg(this);
        dlg.exec();
    });
    connect(m_actSettings, &QAction::triggered, this, &BrowserWindow::showSettings);
    // ---- 阅读模式 / 标签栏 / 手势 ----
    m_actReader = mainMenu->addAction(QStringLiteral("阅读模式"));
    QAction *actTabTop   = mainMenu->addAction(QStringLiteral("标签栏：顶部"));
    QAction *actTabLeft  = mainMenu->addAction(QStringLiteral("标签栏：左侧"));
    QAction *actTabRight = mainMenu->addAction(QStringLiteral("标签栏：右侧"));
    m_actGestures = mainMenu->addAction(QStringLiteral("鼠标手势"));
    m_actGestures->setCheckable(true);
    mainMenu->addSeparator();

    connect(m_actReader, &QAction::triggered, this, &BrowserWindow::toggleReaderMode);
    connect(actTabTop,   &QAction::triggered, this, [this]{ setTabPosition(0); });
    connect(actTabLeft,  &QAction::triggered, this, [this]{ setTabPosition(1); });
    connect(actTabRight, &QAction::triggered, this, [this]{ setTabPosition(2); });
    connect(m_actGestures, &QAction::toggled, this, &BrowserWindow::setMouseGesturesEnabled);

    connect(actQuit, &QAction::triggered, this, &QWidget::close);

    // ---- 快捷键 ----
    auto addShortcut = [this](const QKeySequence &keys, auto slot) {
        auto *sc = new QShortcut(keys, this);
        connect(sc, &QShortcut::activated, this, slot);
    };
    addShortcut(QKeySequence::New,          [this]{ onNewTab(); });      // Ctrl+T
    addShortcut(QKeySequence::Close,        [this]{ if (auto *v = currentView()) onCloseTab(m_tabs->currentIndex()); }); // Ctrl+W
    addShortcut(QKeySequence(QStringLiteral("Ctrl+L")), [this]{ m_urlBar->setFocus(); m_urlBar->selectAll(); });
    addShortcut(QKeySequence::Refresh,      [this]{ navReload(); });     // F5 / Ctrl+R
    addShortcut(QKeySequence(QStringLiteral("Alt+Left")),  [this]{ navBack(); });
    addShortcut(QKeySequence(QStringLiteral("Alt+Right")), [this]{ navForward(); });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+Tab")),  [this]{
        if (m_tabs->count() > 1) m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % m_tabs->count());
    });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Tab")), [this]{
        if (m_tabs->count() > 1) m_tabs->setCurrentIndex((m_tabs->currentIndex() - 1 + m_tabs->count()) % m_tabs->count());
    });

    connect(m_urlBar, &QLineEdit::returnPressed, this, &BrowserWindow::onUrlEntered);

    // ---- 页面查找栏 ----
    m_findBar = new QToolBar(QStringLiteral("Find"), this);
    m_findBar->setMovable(false);
    m_findBar->addWidget(new QLabel(QStringLiteral("查找："), this));
    m_findEdit = new QLineEdit(this);
    m_findEdit->setClearButtonEnabled(true);
    m_findEdit->setMinimumWidth(220);
    m_findBar->addWidget(m_findEdit);
    m_findCountLabel = new QLabel(m_findBar);
    m_findCountLabel->setMinimumWidth(70);
    m_findBar->addWidget(m_findCountLabel);
    QAction *findPrevAct = m_findBar->addAction(QStringLiteral("上一个"));
    QAction *findNextAct = m_findBar->addAction(QStringLiteral("下一个"));
    QAction *findCloseAct = m_findBar->addAction(QStringLiteral("关闭"));
    connect(findPrevAct, &QAction::triggered, this, &BrowserWindow::findPrevious);
    connect(findNextAct, &QAction::triggered, this, &BrowserWindow::findNext);
    connect(findCloseAct, &QAction::triggered, this, &BrowserWindow::hideFindBar);
    connect(m_findEdit, &QLineEdit::returnPressed, this, &BrowserWindow::findNext);
    connect(m_findEdit, &QLineEdit::textChanged, this, [this](const QString &t){
        auto *v = currentView();
        if (!v)
            return;
        if (t.isEmpty()) {
            v->findText(QString());   // 清除高亮
            if (m_findCountLabel)
                m_findCountLabel->clear();
            return;
        }
        v->findText(t, QWebEnginePage::FindFlags(),
                    [this](const QWebEngineFindTextResult &result) {
            if (!m_findCountLabel)
                return;
            const int n = result.numberOfMatches();
            if (n <= 0)
                m_findCountLabel->setText(QStringLiteral("无匹配"));
            else
                m_findCountLabel->setText(QStringLiteral("%1/%2")
                    .arg(result.activeMatch()).arg(n));
        });
    });
    m_findBar->hide();
    addToolBarBreak();
    addToolBar(m_findBar);

    // Ctrl+F / Esc
    addShortcut(QKeySequence::Find, [this]{ showFindBar(); });
    {
        auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), m_findEdit);
        connect(esc, &QShortcut::activated, this, &BrowserWindow::hideFindBar);
    }

    // ---- 页面缩放快捷键 ----
    addShortcut(QKeySequence::ZoomIn,  [this]{ zoomIn(); });     // Ctrl+=
    addShortcut(QKeySequence::ZoomOut, [this]{ zoomOut(); });    // Ctrl+-
    addShortcut(QKeySequence(QStringLiteral("Ctrl+0")), [this]{ zoomReset(); });

    addShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+N")), [this]{ onNewPrivateTab(); });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")), [this]{ onReopenClosedTab(); });
    addShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")), [this]{ showTabSwitcher(); });

    // 跟随系统时，响应系统主题变化
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme) {
                if (themeMode() == QStringLiteral("system"))
                    applyTheme();
            });

    // ---- 打印 / 导出 PDF ----
    addShortcut(QKeySequence::Print, [this]{ printPage(); });  // Ctrl+P
    addShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")), [this]{ savePageAsPdf(); });

    // ---- 盾牌：拦截计数 ----
    connect(m_adBlocker, &AdBlocker::blockedCountChanged, this,
            [this](const QString &host, int count) {
                auto *v = currentView();
                if (v && v->url().host().compare(host, Qt::CaseInsensitive) == 0)
                    m_shieldLabel->setText(QStringLiteral("🛡 %1").arg(count));
            });
    refreshShieldForCurrent();
    connect(m_tabs, &QTabWidget::currentChanged, this, [this]{ refreshShieldForCurrent(); });
}

// ===================== 书签 =====================

QString BrowserWindow::bookmarksFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/bookmarks.json");
}

void BrowserWindow::setupBookmarks()
{
    // 书签栏（单独一行工具条）
    m_bookmarkBar = new QToolBar(QStringLiteral("Bookmarks"), this);
    m_bookmarkBar->setMovable(false);
    m_bookmarkBar->setIconSize(QSize(16, 16));
    addToolBarBreak();
    addToolBar(m_bookmarkBar);

    loadBookmarks();
    rebuildBookmarkBar();

    // 极简模式：书签栏默认隐藏
    m_bookmarkBar->setVisible(false);
}

void BrowserWindow::loadBookmarks()
{
    m_bookmarks.clear();
    QFile f(bookmarksFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        Bookmark b;
        b.title = o.value(QStringLiteral("title")).toString();
        b.url   = QUrl(o.value(QStringLiteral("url")).toString());
        b.group = o.value(QStringLiteral("group")).toString();
        if (b.url.isValid())
            m_bookmarks.append(b);
    }
}

void BrowserWindow::saveBookmarks() const
{
    QJsonArray arr;
    for (const Bookmark &b : m_bookmarks) {
        QJsonObject o;
        o.insert(QStringLiteral("title"), b.title);
        o.insert(QStringLiteral("url"), b.url.toString());
        if (!b.group.isEmpty())
            o.insert(QStringLiteral("group"), b.group);
        arr.append(o);
    }
    QFile f(bookmarksFilePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
        f.close();
    }
}

void BrowserWindow::rebuildBookmarkBar()
{
    if (!m_bookmarkBar)
        return;
    m_bookmarkBar->clear();

    // 未分组的书签：直接作为按钮
    for (const Bookmark &b : m_bookmarks) {
        if (!b.group.isEmpty())
            continue;
        addBookmarkAction(b, m_bookmarkBar);
    }

    // 分组书签：每组一个带下拉菜单的按钮
    QStringList groups;
    for (const Bookmark &b : m_bookmarks) {
        if (!b.group.isEmpty() && !groups.contains(b.group))
            groups << b.group;
    }
    for (const QString &g : groups) {
        auto *menu = new QMenu(g, m_bookmarkBar);
        for (const Bookmark &b : m_bookmarks) {
            if (b.group != g)
                continue;
            const QString text = b.title.isEmpty() ? b.url.host() : b.title;
            QAction *a = menu->addAction(text);
            a->setToolTip(b.url.toString());
            const QUrl url = b.url;
            const QString urlKey = url.toString();
            if (m_faviconCache.contains(urlKey)) {
                const QIcon ico = m_faviconCache.value(urlKey);
                if (!ico.isNull())
                    a->setIcon(ico);
            } else {
                QWebEngineProfile::defaultProfile()->requestIconForPageURL(
                    url, 16, [this, a, urlKey](const QIcon &icon, const QUrl &, const QUrl &) {
                        if (!icon.isNull()) {
                            m_faviconCache.insert(urlKey, icon);
                            a->setIcon(icon);
                        }
                    });
            }
            connect(a, &QAction::triggered, this, [this, url]{ openBookmark(url); });
        }
        auto *btn = new QToolButton(m_bookmarkBar);
        btn->setText(QStringLiteral("📁 ") + g);
        btn->setMenu(menu);
        btn->setPopupMode(QToolButton::InstantPopup);
        m_bookmarkBar->addWidget(btn);
    }
}

void BrowserWindow::addBookmarkAction(const Bookmark &b, QToolBar *bar)
{
    const QString text = b.title.isEmpty() ? b.url.host() : b.title;
    QAction *act = bar->addAction(text);
    act->setToolTip(b.url.toString());
    // favicon：先查缓存；未命中异步请求
    const QUrl url = b.url;
    const QString urlKey = url.toString();
    if (m_faviconCache.contains(urlKey)) {
        const QIcon ico = m_faviconCache.value(urlKey);
        if (!ico.isNull())
            act->setIcon(ico);
    } else {
        QWebEngineProfile::defaultProfile()->requestIconForPageURL(
            url, 16, [this, act, urlKey](const QIcon &icon, const QUrl &, const QUrl &) {
                if (!icon.isNull()) {
                    m_faviconCache.insert(urlKey, icon);
                    act->setIcon(icon);
                }
            });
    }

    connect(act, &QAction::triggered, this, [this, url]{ openBookmark(url); });

    // 用自定义按钮替换默认工具按钮，以支持中键后台打开
    auto *btn = new BookmarkButton(url, bar);
    btn->setDefaultAction(act);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    bar->removeAction(act);
    bar->addWidget(btn);

    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QToolButton::customContextMenuRequested, this,
            [this, url, btn](const QPoint &pos) {
                QMenu menu;
                QAction *edit = menu.addAction(QStringLiteral("编辑书签…"));
                QAction *del = menu.addAction(QStringLiteral("删除书签"));
                QAction *chosen = menu.exec(btn->mapToGlobal(pos));
                if (chosen == edit)
                    editBookmark(url);
                else if (chosen == del)
                    removeBookmark(url);
            });
    connect(btn, &BookmarkButton::middleClicked, this,
            [this](const QUrl &u) { createTab(u, false); });   // 后台打开
}

void BrowserWindow::addBookmarkForCurrentPage()
{
    WebView *v = currentView();
    if (!v)
        return;
    const QUrl url = v->url();
    if (!url.isValid() || url.isEmpty())
        return;

    // 已存在则不重复添加
    for (const Bookmark &b : m_bookmarks) {
        if (b.url == url) {
            QMessageBox::information(this, QStringLiteral("Breeze"),
                                     QStringLiteral("该书签已存在。"));
            return;
        }
    }

    QString title = v->title();
    if (title.isEmpty())
        title = url.host();

    // 询问分组（可留空，也可从已有分组里选）
    QStringList existingGroups;
    for (const Bookmark &b : m_bookmarks) {
        if (!b.group.isEmpty() && !existingGroups.contains(b.group))
            existingGroups << b.group;
    }
    bool ok = false;
    const QString group = QInputDialog::getItem(
        this, QStringLiteral("添加书签"),
        QStringLiteral("分组（可留空）："),
        QStringList{ QString() } + existingGroups,
        0, true, &ok).trimmed();
    if (!ok)
        return;

    Bookmark nb;
    nb.title = title;
    nb.url = url;
    nb.group = group;
    m_bookmarks.append(nb);
    saveBookmarks();
    rebuildBookmarkBar();
}

void BrowserWindow::openBookmark(const QUrl &url)
{
    createTab(url);
}

void BrowserWindow::showBookmarkManager()
{
    BookmarkManager dlg(this);
    dlg.setBookmarks(&m_bookmarks);
    connect(&dlg, &BookmarkManager::changed, this, [this]() {
        saveBookmarks();
        rebuildBookmarkBar();
    });
    dlg.exec();
}

void BrowserWindow::importBookmarks()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入书签"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("HTML 书签 (*.html *.htm);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("无法打开文件。"));
        return;
    }
    const QString html = QString::fromUtf8(f.readAll());
    f.close();

    // 解析 Netscape Bookmark 格式的 <A HREF="...">title</A>
    QRegularExpression re(
        QStringLiteral("<a\\s+href=\"([^\"]+)\"[^>]*>(.*?)</a>"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto it = re.globalMatch(html);

    int added = 0;
    while (it.hasNext()) {
        const auto m = it.next();
        const QString href = m.captured(1).trimmed();
        QString title = m.captured(2).trimmed();
        title.remove(QRegularExpression(QStringLiteral("<[^>]+>")));  // 去标签
        if (href.isEmpty() || href.startsWith(QLatin1String("javascript:")))
            continue;
        const QUrl url(href);
        if (!url.isValid())
            continue;
        // 去重
        bool exists = false;
        for (const Bookmark &b : m_bookmarks) {
            if (b.url == url) { exists = true; break; }
        }
        if (exists)
            continue;
        Bookmark nb;
        nb.title = title.isEmpty() ? url.host() : title;
        nb.url = url;
        m_bookmarks.append(nb);
        ++added;
    }

    saveBookmarks();
    rebuildBookmarkBar();
    QMessageBox::information(this, QStringLiteral("导入完成"),
        QStringLiteral("新增 %1 个书签。").arg(added));
}

void BrowserWindow::exportBookmarks()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出书签"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/breeze-bookmarks.html"),
        QStringLiteral("HTML 书签 (*.html)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件。"));
        return;
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n"
       << "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n"
       << "<TITLE>Bookmarks</TITLE>\n"
       << "<H1>Bookmarks</H1>\n"
       << "<DL><p>\n";
    for (const Bookmark &b : m_bookmarks) {
        const QString title = b.title.isEmpty() ? b.url.host() : b.title;
        ts << "    <DT><A HREF=\"" << b.url.toString().toHtmlEscaped() << "\">"
           << title.toHtmlEscaped() << "</A>\n";
    }
    ts << "</DL><p>\n";
    f.close();

    QMessageBox::information(this, QStringLiteral("导出完成"),
        QStringLiteral("已导出 %1 个书签。").arg(m_bookmarks.size()));
}



void BrowserWindow::removeBookmark(const QUrl &url)
{
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks.at(i).url == url) {
            m_bookmarks.removeAt(i);
            break;
        }
    }
    saveBookmarks();
    rebuildBookmarkBar();
}

void BrowserWindow::editBookmark(const QUrl &url)
{
    int idx = -1;
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks.at(i).url == url) { idx = i; break; }
    }
    if (idx < 0)
        return;

    bool ok = false;
    const QString newTitle = QInputDialog::getText(
        this, QStringLiteral("编辑书签"), QStringLiteral("标题："),
        QLineEdit::Normal, m_bookmarks.at(idx).title, &ok);
    if (!ok)
        return;

    // 收集已有分组供选择
    QStringList groups;
    for (const Bookmark &b : m_bookmarks)
        if (!b.group.isEmpty() && !groups.contains(b.group))
            groups << b.group;
    groups.prepend(QString());   // 空 = 未分组

    const QString newGroup = QInputDialog::getItem(
        this, QStringLiteral("编辑书签"), QStringLiteral("分组："),
        groups, groups.indexOf(m_bookmarks.at(idx).group), true, &ok);
    if (!ok)
        return;

    m_bookmarks[idx].title = newTitle;
    m_bookmarks[idx].group = newGroup;
    saveBookmarks();
    rebuildBookmarkBar();
    statusBar()->showMessage(QStringLiteral("书签已更新"), 2000);
}

// ===================== 下载 =====================

void BrowserWindow::setupDownloads()
{
    m_downloadManager = new DownloadManager(this);

    auto *profile = QWebEngineProfile::defaultProfile();
    connect(profile, &QWebEngineProfile::downloadRequested, this,
            [this](QWebEngineDownloadRequest *download) {
                if (!download)
                    return;

                // .user.js → 走在线安装流程（不落盘为下载）
                const QString name = download->downloadFileName();
                const QUrl srcUrl = download->url();
                if (name.endsWith(QStringLiteral(".user.js"))
                    || srcUrl.toString().endsWith(QStringLiteral(".user.js"))) {
                    download->cancel();
                    installUserScriptFromUrl(srcUrl);
                    return;
                }

                // 使用系统"下载"目录，避免 Program Files 下无写权限
                QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
                if (dir.isEmpty())
                    dir = QDir::homePath();
                QDir().mkpath(dir);
                download->setDownloadDirectory(dir);
                download->accept();

                m_downloadManager->addDownload(download);
            });
}

void BrowserWindow::showDownloads()
{
    if (m_downloadManager)
        m_downloadManager->show();
}

void BrowserWindow::installUserScriptFromUrl(const QUrl &url)
{
    statusBar()->showMessage(QStringLiteral("正在获取用户脚本…"), 3000);

    auto *nam = new QNetworkAccessManager(this);
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Breeze"));
    QNetworkReply *reply = nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, url]() {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, QStringLiteral("安装失败"),
                                 QStringLiteral("无法获取脚本：%1").arg(reply->errorString()));
            return;
        }

        UserScript s;
        s.code = QString::fromUtf8(reply->readAll());
        s.name = url.fileName();
        UserScriptManager::parseMetadata(s);

        const QString info = QStringLiteral("名称：%1\n匹配：%2\n运行时机：%3\n\n是否安装该用户脚本？")
            .arg(s.name.isEmpty() ? url.fileName() : s.name,
                 s.match.isEmpty() ? QStringLiteral("(未指定)") : s.match,
                 s.runAt);

        if (QMessageBox::question(this, QStringLiteral("安装用户脚本"), info)
            != QMessageBox::Yes)
            return;

        QList<UserScript> scripts = UserScriptManager::loadScripts();
        scripts.append(s);
        UserScriptManager::saveScripts(scripts);

        statusBar()->showMessage(QStringLiteral("已安装用户脚本：%1").arg(s.name), 3000);
    });
}

// ===================== 历史记录 =====================

QString BrowserWindow::historyFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/history.json");
}

void BrowserWindow::setupHistory()
{
    m_historyDialog = new HistoryDialog(this);

    connect(m_historyDialog, &HistoryDialog::openUrlRequested, this,
            [this](const QUrl &url){ createTab(url); });
    connect(m_historyDialog, &HistoryDialog::cleared, this, [this]() {
        m_history.clear();
        saveHistory();
    });

    loadHistory();
    m_historyDialog->setEntries(m_history);
}

void BrowserWindow::loadHistory()
{
    m_history.clear();
    QFile f(historyFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        HistoryEntry e;
        e.title = o.value(QStringLiteral("title")).toString();
        e.url   = QUrl(o.value(QStringLiteral("url")).toString());
        e.visitedAt = QDateTime::fromString(
            o.value(QStringLiteral("time")).toString(), Qt::ISODate);
        if (e.url.isValid())
            m_history.append(e);
    }
}

void BrowserWindow::saveHistory() const
{
    // 只保留最近 500 条
    const int maxEntries = 500;
    QJsonArray arr;
    int count = 0;
    for (const HistoryEntry &e : m_history) {
        if (count++ >= maxEntries)
            break;
        QJsonObject o;
        o.insert(QStringLiteral("title"), e.title);
        o.insert(QStringLiteral("url"), e.url.toString());
        o.insert(QStringLiteral("time"), e.visitedAt.toString(Qt::ISODate));
        arr.append(o);
    }
    QFile f(historyFilePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
        f.close();
    }
}

void BrowserWindow::recordHistory(WebView *view)
{
    if (!SettingsDialog::recordHistory())
        return;

    WebView *v = view ? view : currentView();
    if (!v)
        return;
    if (v->property("breezePrivate").toBool())
        return;
    const QUrl url = v->url();
    if (!url.isValid() || url.isEmpty() || url.scheme() == QStringLiteral("about"))
        return;

    HistoryEntry e;
    e.url = url;
    e.title = v->title().isEmpty() ? url.host() : v->title();
    e.visitedAt = QDateTime::currentDateTime();

    // 若与最近一条相同 URL，则更新而不是重复插入
    if (!m_history.isEmpty() && m_history.first().url == url) {
        m_history.first() = e;
    } else {
        m_history.prepend(e);
    }

    if (m_historyDialog) {
        m_historyDialog->setEntries(m_history);
    }
    saveHistory();
}

void BrowserWindow::showHistory()
{
    if (!m_historyDialog)
        return;
    m_historyDialog->setEntries(m_history);
    m_historyDialog->show();
    m_historyDialog->raise();
    m_historyDialog->activateWindow();
}

void BrowserWindow::importHistory()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("导入历史"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("JSON 文件 (*.json);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("无法打开文件。"));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isArray()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"),
                             QStringLiteral("文件格式无效（应为 JSON 数组）。"));
        return;
    }

    int added = 0;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QUrl url(o.value(QStringLiteral("url")).toString());
        if (!url.isValid())
            continue;
        bool exists = false;
        for (const HistoryEntry &e : m_history) {
            if (e.url == url) { exists = true; break; }
        }
        if (exists)
            continue;
        HistoryEntry e;
        e.url = url;
        e.title = o.value(QStringLiteral("title")).toString();
        e.visitedAt = QDateTime::fromString(
            o.value(QStringLiteral("time")).toString(), Qt::ISODate);
        m_history.prepend(e);
        ++added;
    }

    saveHistory();
    if (m_historyDialog)
        m_historyDialog->setEntries(m_history);
    QMessageBox::information(this, QStringLiteral("导入完成"),
        QStringLiteral("新增 %1 条历史记录。").arg(added));
}



// ===================== 阅读模式 =====================

void BrowserWindow::toggleReaderMode()
{
    WebView *v = currentView();
    if (!v)
        return;

    const QString js = QStringLiteral(R"JS(
(function() {
  var old = document.getElementById('breeze-reader');
  if (old) { old.remove(); document.body.style.overflow=''; return 'off'; }
  var best = null, bestLen = 0;
  document.querySelectorAll('article, main, [role=main], .article, .post, .content, #content').forEach(function(el){
    var n = (el.innerText||'').length;
    if (n > bestLen) { bestLen = n; best = el; }
  });
  if (!best || bestLen < 200) { best = document.body; bestLen = (best.innerText||'').length; }
  if (bestLen < 200) return 'no-content';
  var text = best.innerText || '';
  var overlay = document.createElement('div');
  overlay.id = 'breeze-reader';
  overlay.style.cssText = 'position:fixed;inset:0;z-index:2147483647;background:#f5f2ea;color:#222;overflow:auto;';
  var fontSize = 19;
  var bgIndex = 0;
  var bgs = ['#f5f2ea', '#ffffff', '#e8f0e8', '#2b2b2b'];
  var fgs = ['#222', '#222', '#222', '#ddd'];
  var bar = document.createElement('div');
  bar.style.cssText = 'position:sticky;top:0;background:inherit;padding:8px 16px;text-align:right;';
  function mkBtn(txt, fn){
    var b = document.createElement('button');
    b.textContent = txt;
    b.style.cssText = 'padding:6px 12px;margin-left:6px;border:1px solid #bbb;border-radius:6px;background:#fff;color:#333;cursor:pointer;';
    b.onclick = fn;
    return b;
  }
  bar.appendChild(mkBtn('A-', function(){ fontSize = Math.max(12, fontSize - 2); inner.style.fontSize = fontSize + 'px'; }));
  bar.appendChild(mkBtn('A+', function(){ fontSize = Math.min(40, fontSize + 2); inner.style.fontSize = fontSize + 'px'; }));
  bar.appendChild(mkBtn('背景', function(){
    bgIndex = (bgIndex + 1) % bgs.length;
    overlay.style.background = bgs[bgIndex];
    overlay.style.color = fgs[bgIndex];
    inner.style.color = fgs[bgIndex];
  }));
  bar.appendChild(mkBtn('关闭阅读模式 (Esc)', function(){ overlay.remove(); document.body.style.overflow=''; }));
  var inner = document.createElement('div');
  inner.style.cssText = 'max-width:760px;margin:0 auto;padding:20px 32px 60px;font-family:Georgia,"Microsoft YaHei",serif;font-size:19px;line-height:1.9;';
  var h = document.createElement('h1');
  h.textContent = document.title || '';
  h.style.cssText = 'font-size:28px;line-height:1.4;margin:0 0 24px;';
  var body = document.createElement('div');
  body.textContent = text;
  body.style.cssText = 'white-space:pre-wrap;word-wrap:break-word;';
  inner.appendChild(h); inner.appendChild(body);
  overlay.appendChild(bar); overlay.appendChild(inner);
  document.body.appendChild(overlay);
  document.body.style.overflow = 'hidden';
  document.addEventListener('keydown', function esc(e){
    if (e.key === 'Escape') { overlay.remove(); document.body.style.overflow=''; document.removeEventListener('keydown', esc); }
  });
  return 'on';
})();
)JS");

    v->page()->runJavaScript(js, [this](const QVariant &r) {
        const QString s = r.toString();
        if (s == QLatin1String("on"))
            statusBar()->showMessage(QStringLiteral("已进入阅读模式（Esc 退出）"), 3000);
        else if (s == QLatin1String("off"))
            statusBar()->showMessage(QStringLiteral("已退出阅读模式"), 2000);
        else
            statusBar()->showMessage(QStringLiteral("当前页面没有可提取的正文"), 3000);
    });
}

// ===================== 标签栏位置 =====================

void BrowserWindow::setTabPosition(int pos)
{
    QTabWidget::TabPosition p = QTabWidget::North;
    if (pos == 1) p = QTabWidget::West;
    else if (pos == 2) p = QTabWidget::East;
    m_tabs->setTabPosition(p);

    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("ui/tabPosition"), pos);
    statusBar()->showMessage(
        pos == 0 ? QStringLiteral("标签栏：顶部")
        : pos == 1 ? QStringLiteral("标签栏：左侧")
                   : QStringLiteral("标签栏：右侧"), 2000);
}

// ===================== 鼠标手势 =====================

void BrowserWindow::setMouseGesturesEnabled(bool enabled)
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("ui/mouseGestures"), enabled);

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *v = qobject_cast<WebView *>(m_tabs->widget(i));
        if (!v)
            continue;
        const QString js = enabled
            ? QStringLiteral(R"JS(
(function(){
  if (window.__breezeGestures) return;
  window.__breezeGestures = true;
  var sx=0, sy=0, tracking=false;
  document.addEventListener('mousedown', function(e){ if(e.button===2){ tracking=true; sx=e.clientX; sy=e.clientY; } }, true);
  document.addEventListener('mouseup', function(e){
    if (e.button!==2 || !tracking) return;
    tracking=false;
    var dx=e.clientX-sx, dy=e.clientY-sy, ax=Math.abs(dx), ay=Math.abs(dy);
    if (Math.max(ax,ay) < 60) return;
    if (ax > ay) { if (dx < 0) history.back(); else history.forward(); }
    else if (dy < 0) location.reload();
    else window.scrollTo({top:0, behavior:'smooth'});   // 下滑：回到顶部
  }, true);
})();
)JS")
            : QStringLiteral("window.__breezeGestures = false;");
        v->page()->runJavaScript(js);
    }
    statusBar()->showMessage(
        enabled ? QStringLiteral("鼠标手势已启用（左滑后退 / 右滑前进 / 上滑刷新 / 下滑回顶）")
                : QStringLiteral("鼠标手势已关闭"), 3000);
}

void BrowserWindow::exportHistory()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出历史"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/breeze-history.json"),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty())
        return;

    QJsonArray arr;
    for (const HistoryEntry &e : m_history) {
        QJsonObject o;
        o.insert(QStringLiteral("title"), e.title);
        o.insert(QStringLiteral("url"), e.url.toString());
        o.insert(QStringLiteral("time"), e.visitedAt.toString(Qt::ISODate));
        arr.append(o);
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QStringLiteral("无法写入文件。"));
        return;
    }
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    f.close();

    QMessageBox::information(this, QStringLiteral("导出完成"),
        QStringLiteral("已导出 %1 条历史记录。").arg(m_history.size()));
}



WebView *BrowserWindow::currentView() const
{
    return qobject_cast<WebView *>(m_tabs->currentWidget());
}

QWebEngineProfile *BrowserWindow::privateProfile()
{
    if (!m_privateProfile) {
        // 独立、非持久化的 profile：不写盘、不存 Cookie
        m_privateProfile = new QWebEngineProfile(this);
        m_privateProfile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
        m_privateProfile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
        m_privateProfile->setPersistentStoragePath(QString());   // 不落盘
    }
    return m_privateProfile;
}

void BrowserWindow::onNewPrivateTab()
{
    createTabView(true);
    m_urlBar->setFocus();
    m_urlBar->selectAll();
}

QString BrowserWindow::themeMode() const
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    return s.value(QStringLiteral("theme/mode"), QStringLiteral("system")).toString();
}

void BrowserWindow::setThemeMode(const QString &mode)
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("theme/mode"), mode);
    applyTheme();
}

void BrowserWindow::applyTheme()
{
    const QString mode = themeMode();

    bool dark = false;
    if (mode == QStringLiteral("dark")) {
        dark = true;
    } else if (mode == QStringLiteral("light")) {
        dark = false;
    } else {
        // 跟随系统
        dark = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    }

    if (dark) {
        qApp->setStyleSheet(QStringLiteral(R"(
            QMainWindow, QDialog { background: #2b2b2b; color: #ddd; }
            QToolBar { background: #333; border: none; spacing: 2px; }
            QLineEdit { background: #3c3c3c; color: #eee; border: 1px solid #555; border-radius: 3px; padding: 2px 6px; }
            QTabWidget::pane { border: 1px solid #444; }
            QTabBar::tab { background: #3a3a3a; color: #ccc; padding: 4px 10px; }
            QTabBar::tab:selected { background: #2b2b2b; color: #fff; }
            QListWidget, QTreeWidget, QTableWidget { background: #333; color: #ddd; alternate-background-color: #3a3a3a; }
            QHeaderView::section { background: #3a3a3a; color: #ddd; border: 1px solid #444; }
            QMenu { background: #333; color: #ddd; }
            QMenu::item:selected { background: #4a6fa5; }
            QPushButton { background: #3c3c3c; color: #eee; border: 1px solid #555; border-radius: 3px; padding: 3px 10px; }
            QPushButton:hover { background: #484848; }
            QStatusBar { background: #333; color: #bbb; }
            QProgressBar { border: 1px solid #555; background: #333; }
            QProgressBar::chunk { background: #4a6fa5; }
        )"));
        QPalette p;
        p.setColor(QPalette::Window, QColor(43, 43, 43));
        p.setColor(QPalette::WindowText, Qt::white);
        p.setColor(QPalette::Base, QColor(51, 51, 51));
        p.setColor(QPalette::AlternateBase, QColor(58, 58, 58));
        p.setColor(QPalette::Text, Qt::white);
        p.setColor(QPalette::Button, QColor(60, 60, 60));
        p.setColor(QPalette::ButtonText, Qt::white);
        p.setColor(QPalette::Highlight, QColor(74, 111, 165));
        qApp->setPalette(p);
    } else {
        qApp->setStyleSheet(QString());
        qApp->setPalette(qApp->style()->standardPalette());
    }

    // 更新菜单勾选状态
    if (m_actThemeSystem) {
        m_actThemeSystem->setChecked(mode == QStringLiteral("system"));
        m_actThemeLight->setChecked(mode == QStringLiteral("light"));
        m_actThemeDark->setChecked(mode == QStringLiteral("dark"));
    }
}



WebView *BrowserWindow::createTabView(bool privateMode)
{
    WebView *view = privateMode
        ? new WebView(privateProfile(), this)
        : new WebView(this);
    view->setProperty("breezePrivate", privateMode);
    view->setAdBlocker(m_adBlocker);

    // 允许网页通过脚本打开新窗口（由 createWindow 回调接管）
    view->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, true);

    // 新窗口请求：向本窗口索取一个已加入标签栏的新 WebView，
    // Qt 会把新窗口内容加载进该视图（真正打开新标签）
    view->setNewTabProvider([this](bool background) -> WebView * {
        WebView *nv = createTabView();
        if (background) {
            // createTabView 内已 setCurrentIndex(index)；若需后台打开，切回原标签
            const int idx = m_tabs->indexOf(nv);
            if (idx >= 0 && m_tabs->count() > 1)
                m_tabs->setCurrentIndex(idx - 1 < 0 ? idx + 1 : idx - 1);
        }
        return nv;
    });

    // 右键菜单 -> AI 处理选中文字
    connect(view, &WebView::aiActionRequested, this,
            [this](const QString &action, const QString &text) {
                const QString prompt = (action == QStringLiteral("translate"))
                    ? QStringLiteral("请把下面的内容翻译成简体中文（若已是中文则翻译成英文）：\n\n") + text
                    : (action == QStringLiteral("rewrite"))
                    ? QStringLiteral("请改写下面的内容，使表达更清晰自然：\n\n") + text
                    : QStringLiteral("请用简体中文解释下面的内容：\n\n") + text;
                AiDialog dlg(this);
                dlg.askWithPrompt(prompt);
                dlg.exec();
            });

    // 划词工具栏 -> 解释 / 翻译 / 搜索
    connect(view, &WebView::selectionActionRequested, this,
            [this](const QString &action, const QString &text) {
                if (action == QStringLiteral("search")) {
                    const QString tmpl = SettingsDialog::searchUrlTemplate(SettingsDialog::searchEngine());
                    const QString query = QString::fromUtf8(QUrl::toPercentEncoding(text));
                    createTab(QUrl(tmpl.arg(query)), true);
                    return;
                }
                const QString prompt = (action == QStringLiteral("translate"))
                    ? QStringLiteral("请把下面的内容翻译成简体中文（若已是中文则翻译成英文）：\n\n") + text
                    : QStringLiteral("请用简体中文解释下面的内容：\n\n") + text;
                AiDialog dlg(this);
                dlg.askWithPrompt(prompt);
                dlg.exec();
            });

    // 右键菜单 -> 用其他搜索引擎搜索选中文字
    connect(view, &WebView::searchRequested, this,
            [this](const QString &engine, const QString &text) {
                const QString tmpl = SettingsDialog::searchUrlTemplate(engine);
                const QString query = QString::fromUtf8(QUrl::toPercentEncoding(text));
                createTab(QUrl(tmpl.arg(query)), true);
            });

    connect(view, &QWebEngineView::loadStarted, this, &BrowserWindow::onLoadStarted);
    connect(view, &QWebEngineView::loadProgress, this, &BrowserWindow::onLoadProgress);
    connect(view, &QWebEngineView::loadFinished, this, &BrowserWindow::onLoadFinished);

    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &title) {
        Q_UNUSED(title);
        updateTabTitle(view);
    });
    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &u) {
        updateTabUrl(view, u);
    });

    // 悬停链接：状态栏显示目标地址（仅当前标签响应）
    connect(view, &WebView::hoverUrlChanged, this, [this, view](const QString &u) {
        if (view != currentView())
            return;
        if (u.isEmpty())
            statusBar()->clearMessage();
        else
            statusBar()->showMessage(u);
    });

    const int index = m_tabs->addTab(view,
        privateMode ? QStringLiteral("隐私") : QStringLiteral("新标签页"));
    m_tabs->setCurrentIndex(index);
    return view;
}

WebView *BrowserWindow::createTab(const QUrl &url, bool switchToTab)
{
    WebView *view = createTabView();
    const int index = m_tabs->indexOf(view);

    if (!switchToTab && index >= 0 && m_tabs->count() > 1)
        m_tabs->setCurrentIndex(index - 1 < 0 ? index + 1 : index - 1);

    if (url.isValid() && !url.isEmpty()) {
        // 先注册 document-start 脚本与扩展 content scripts，再导航
        injectStartScripts(view, url);
        injectExtensionScripts(view, url);
        view->setUrl(url);
    }

    return view;
}

void BrowserWindow::onNewTab()
{
    WebView *view = createTab(QUrl(), true);
    if (view)
        view->setHtml(dialsHtml());
    m_urlBar->setFocus();
    m_urlBar->selectAll();
}

void BrowserWindow::onCloseTab(int index)
{
    QWidget *w = m_tabs->widget(index);
    // 固定标签不允许关闭
    if (w && m_pinnedTabs.contains(w)) {
        statusBar()->showMessage(QStringLiteral("该标签已固定，请先取消固定"), 2000);
        return;
    }
    if (auto *v = qobject_cast<WebView *>(w)) {
        const QUrl u = v->url();
        // 不记录空白页与隐私标签
        if (u.isValid() && !u.isEmpty()
            && !v->property("breezePrivate").toBool()) {
            m_closedTabs.prepend(u);
            while (m_closedTabs.size() > 20)
                m_closedTabs.removeLast();
        }
    }
    m_tabs->removeTab(index);
    if (w)
        w->deleteLater();

    if (m_tabs->count() == 0)
        onNewTab();
}

void BrowserWindow::onReopenClosedTab()
{
    if (m_closedTabs.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("没有可恢复的标签"), 2000);
        return;
    }
    const QUrl u = m_closedTabs.takeFirst();
    createTab(u, true);
}

void BrowserWindow::onTabBarContextMenu(const QPoint &pos)
{
    const int index = m_tabs->tabBar()->tabAt(pos);
    if (index < 0)
        return;
    auto *view = qobject_cast<WebView *>(m_tabs->widget(index));

    QMenu menu(this);
    if (view) {
        const bool muted = view->page()->isAudioMuted();
        QAction *actMute = menu.addAction(muted ? QStringLiteral("取消静音")
                                                : QStringLiteral("静音此标签"));
        connect(actMute, &QAction::triggered, this, [view, muted]() {
            view->page()->setAudioMuted(!muted);
        });

        const bool pinned = m_pinnedTabs.contains(view);
        QAction *actPin = menu.addAction(pinned ? QStringLiteral("取消固定")
                                                : QStringLiteral("固定标签"));
        connect(actPin, &QAction::triggered, this, [this, index]() { togglePinTab(index); });

        // 定时刷新
        QMenu *refreshMenu = menu.addMenu(QStringLiteral("定时刷新"));
        const int intervals[] = { 0, 30, 60, 300, 600 };
        const QString labels[] = { QStringLiteral("关闭"), QStringLiteral("30 秒"),
                                   QStringLiteral("1 分钟"), QStringLiteral("5 分钟"),
                                   QStringLiteral("10 分钟") };
        for (int i = 0; i < 5; ++i) {
            QAction *a = refreshMenu->addAction(labels[i]);
            const int sec = intervals[i];
            connect(a, &QAction::triggered, this, [this, view, sec]() {
                setTabAutoRefresh(view, sec);
            });
        }
    }
    QAction *actClose = menu.addAction(QStringLiteral("关闭标签"));
    connect(actClose, &QAction::triggered, this, [this, index]() { onCloseTab(index); });

    menu.exec(m_tabs->tabBar()->mapToGlobal(pos));
}

void BrowserWindow::togglePinTab(int index)
{
    auto *view = qobject_cast<WebView *>(m_tabs->widget(index));
    if (!view)
        return;

    QString title = view->title().isEmpty() ? QStringLiteral("新标签页") : view->title();

    if (m_pinnedTabs.contains(view)) {
        m_pinnedTabs.remove(view);
        m_tabs->setTabText(index, title);
        statusBar()->showMessage(QStringLiteral("已取消固定"), 1500);
    } else {
        m_pinnedTabs.insert(view);
        m_tabs->setTabText(index, QStringLiteral("📌 ") + title);
        statusBar()->showMessage(QStringLiteral("已固定标签"), 1500);
    }
}

void BrowserWindow::setTabAutoRefresh(WebView *view, int seconds)
{
    if (!view)
        return;

    // 先清掉已有的定时器
    if (auto *old = m_refreshTimers.take(view)) {
        old->stop();
        old->deleteLater();
    }

    if (seconds <= 0) {
        statusBar()->showMessage(QStringLiteral("已关闭定时刷新"), 1500);
        return;
    }

    auto *timer = new QTimer(this);
    timer->setInterval(seconds * 1000);
    connect(timer, &QTimer::timeout, view, [view]() { view->reload(); });
    // 标签销毁时清理
    connect(view, &QObject::destroyed, this, [this, view]() {
        if (auto *t = m_refreshTimers.take(view)) { t->stop(); t->deleteLater(); }
    });
    timer->start();
    m_refreshTimers.insert(view, timer);

    statusBar()->showMessage(
        QStringLiteral("已设置定时刷新：%1 秒").arg(seconds), 2000);
}

void BrowserWindow::showTabSwitcher()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("切换标签页"));
    dlg.resize(480, 380);

    auto *layout = new QVBoxLayout(&dlg);
    auto *edit = new QLineEdit(&dlg);
    edit->setPlaceholderText(QStringLiteral("输入标题或网址筛选…"));
    edit->setClearButtonEnabled(true);
    layout->addWidget(edit);

    auto *list = new QListWidget(&dlg);
    layout->addWidget(list, 1);

    // 填充所有标签
    auto fill = [this, list](const QString &filter) {
        list->clear();
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *v = qobject_cast<WebView *>(m_tabs->widget(i));
            if (!v) continue;
            const QString title = v->title().isEmpty()
                ? QStringLiteral("新标签页") : v->title();
            const QString url = v->url().toString();
            if (!filter.isEmpty()
                && !title.contains(filter, Qt::CaseInsensitive)
                && !url.contains(filter, Qt::CaseInsensitive))
                continue;
            auto *item = new QListWidgetItem(
                QStringLiteral("%1  —  %2").arg(title, url));
            item->setData(Qt::UserRole, i);
            list->addItem(item);
        }
        if (list->count() > 0)
            list->setCurrentRow(0);
    };

    fill(QString());

    connect(edit, &QLineEdit::textChanged, &dlg, [fill](const QString &t) { fill(t); });

    auto activate = [&dlg, list]() {
        auto *item = list->currentItem();
        if (item)
            dlg.done(item->data(Qt::UserRole).toInt() + 1);  // +1 避免 0 与 Rejected 冲突
    };
    connect(edit, &QLineEdit::returnPressed, &dlg, activate);
    connect(list, &QListWidget::itemActivated, &dlg, [&dlg, activate](QListWidgetItem*) { activate(); });

    const int ret = dlg.exec();
    if (ret > 0)
        m_tabs->setCurrentIndex(ret - 1);
}

void BrowserWindow::onTabChanged(int index)
{
    Q_UNUSED(index);
    WebView *view = currentView();
    if (!view) {
        m_urlBar->clear();
        return;
    }
    m_urlBar->setText(view->url().toString());
    m_urlBar->setCursorPosition(0);
    setWindowTitle(view->title().isEmpty()
                       ? QStringLiteral("Breeze ") + QStringLiteral(BREEZE_VERSION)
                       : QStringLiteral("%1 - Breeze ").arg(view->title())
                             + QStringLiteral(BREEZE_VERSION));
    updateNavButtons();
}

void BrowserWindow::onUrlEntered()
{
    WebView *view = currentView();
    if (!view)
        return;

    QString text = m_urlBar->text().trimmed();
    if (text.isEmpty())
        return;

    QUrl url = normalizedUrl(text);
    if (url.isValid()) {
        // 看起来是网址（含协议，或含点的域名，或 localhost），直接访问
        view->setUrl(url);
    } else {
        // 否则当作搜索关键词
        const QString tmpl = SettingsDialog::searchUrlTemplate(SettingsDialog::searchEngine());
        const QString query = QString::fromUtf8(QUrl::toPercentEncoding(text));
        view->setUrl(QUrl(tmpl.arg(query)));
    }
    view->setFocus();
}


QString BrowserWindow::dialsHtml() const
{
    // 收集候选：书签优先，其次最近历史，去重
    QStringList items;
    QSet<QString> seen;
    auto addItem = [&](const QString &title, const QUrl &url) {
        const QString u = url.toString();
        if (u.isEmpty() || seen.contains(u)) return;
        if (!url.scheme().startsWith(QStringLiteral("http"))) return;
        seen.insert(u);
        const QString t = title.isEmpty() ? url.host() : title;
        items << QStringLiteral("<a class=\"tile\" href=\"%1\"><span class=\"host\">%2</span><span class=\"title\">%3</span></a>")
            .arg(u.toHtmlEscaped(),
                 url.host().toHtmlEscaped(),
                 t.toHtmlEscaped());
    };
    for (const Bookmark &b : m_bookmarks) {
        if (items.size() >= 12) break;
        addItem(b.title, b.url);
    }
    for (auto it = m_history.crbegin(); it != m_history.crend() && items.size() < 12; ++it)
        addItem(it->title, it->url);

    const QString tiles = items.isEmpty()
        ? QStringLiteral("<p class=\"empty\">暂无书签或历史。访问网站后会出现在这里。</p>")
        : items.join(QString());

    return QStringLiteral(R"HTML(
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<style>
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100vh; display: flex; flex-direction: column;
    align-items: center; justify-content: center;
    font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
    background: linear-gradient(135deg, #e8f4fd 0%%, #f5f2ea 100%%);
    color: #222;
  }
  h1 { font-weight: 300; font-size: 40px; color: #3a6ea5; margin: 0 0 40px; letter-spacing: 4px; }
  .grid {
    display: grid; grid-template-columns: repeat(4, 1fr);
    gap: 18px; max-width: 720px; padding: 0 20px;
  }
  .tile {
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    width: 150px; height: 110px; padding: 10px;
    background: #fff; border-radius: 12px; text-decoration: none; color: #333;
    box-shadow: 0 2px 8px rgba(0,0,0,0.08); transition: transform .15s, box-shadow .15s;
    overflow: hidden;
  }
  .tile:hover { transform: translateY(-3px); box-shadow: 0 6px 16px rgba(0,0,0,0.15); }
  .host { font-size: 13px; color: #3a6ea5; font-weight: 600; }
  .title { font-size: 12px; color: #888; margin-top: 6px; text-align: center;
           overflow: hidden; text-overflow: ellipsis; white-space: nowrap; max-width: 130px; }
  .empty { color: #999; font-size: 15px; }
</style></head>
<body>
  <h1>Breeze</h1>
  <div class="grid">%1</div>
</body></html>
)HTML").arg(tiles);
}

QUrl BrowserWindow::homeUrl() const
{
    return QUrl(SettingsDialog::homePage());
}

QUrl BrowserWindow::normalizedUrl(const QString &text) const
{
    // 已带协议
    if (text.contains(QStringLiteral("://")))
        return QUrl(text);

    // localhost / IP / 无空格且含点（视为域名）
    const bool looksLikeDomain =
        !text.contains(QLatin1Char(' '))
        && (text.contains(QLatin1Char('.'))
            || text.startsWith(QStringLiteral("localhost"), Qt::CaseInsensitive));

    if (looksLikeDomain) {
        return QUrl(QStringLiteral("https://") + text);
    }

    // 不是网址
    return QUrl();
}

void BrowserWindow::refreshUrlCompleter(const QString &prefix)
{
    if (prefix.trimmed().isEmpty())
        return;

    const QString needle = prefix.trimmed();
    QStringList items;
    QSet<QString> seen;

    auto add = [&](const QString &title, const QUrl &url) {
        if (!url.isValid())
            return;
        const QString key = url.toString();
        if (seen.contains(key))
            return;
        seen.insert(key);
        const QString label = title.isEmpty()
            ? key
            : QStringLiteral("%1 — %2").arg(title, key);
        items << label;
    };

    int n = 0;
    for (auto it = m_history.crbegin(); it != m_history.crend() && n < 300; ++it, ++n) {
        if (it->url.toString().contains(needle, Qt::CaseInsensitive)
            || it->title.contains(needle, Qt::CaseInsensitive))
            add(it->title, it->url);
    }
    n = 0;
    for (const Bookmark &b : m_bookmarks) {
        if (n++ >= 300) break;
        if (b.url.toString().contains(needle, Qt::CaseInsensitive)
            || b.title.contains(needle, Qt::CaseInsensitive))
            add(b.title, b.url);
    }

    if (!m_completer)
        return;
    auto *model = new QStringListModel(items, m_completer);
    m_completer->setModel(model);
}

void BrowserWindow::checkForUpdates()
{
    if (!m_updateManager)
        m_updateManager = new UpdateManager(this);

    statusBar()->showMessage(QStringLiteral("正在检查更新…"), 3000);

    // 避免重复连接
    static bool connected = false;
    if (!connected) {
        connected = true;
        connect(m_updateManager, &UpdateManager::updateAvailable, this,
                [this](const QString &version, const QString &url, const QString &notes) {
                    const QString msg = QStringLiteral("发现新版本 %1（当前 %2）。\n\n%3\n\n是否打开发布页？")
                        .arg(version, QStringLiteral(BREEZE_VERSION), notes.left(500));
                    if (QMessageBox::question(this, QStringLiteral("检查更新"), msg) == QMessageBox::Yes)
                        QDesktopServices::openUrl(QUrl(url));
                });
        connect(m_updateManager, &UpdateManager::upToDate, this,
                [this](const QString &v) {
                    QMessageBox::information(this, QStringLiteral("检查更新"),
                        QStringLiteral("已是最新版本 %1。").arg(v));
                });
        connect(m_updateManager, &UpdateManager::checkFailed, this,
                [this](const QString &err) {
                    QMessageBox::warning(this, QStringLiteral("检查更新失败"), err);
                });
    }

    m_updateManager->checkForUpdates();
}

void BrowserWindow::refreshShieldForCurrent()
{
    if (!m_shieldLabel || !m_adBlocker)
        return;
    auto *v = currentView();
    if (!v) {
        m_shieldLabel->setText(QStringLiteral("🛡 0"));
        return;
    }
    const QString host = v->url().host().toLower();
    m_shieldLabel->setText(QStringLiteral("🛡 %1").arg(m_adBlocker->blockedCountForHost(host)));
}

void BrowserWindow::showBlockedDetails()
{
    auto *v = currentView();
    if (!v || !m_adBlocker)
        return;
    const QString host = v->url().host().toLower();
    const QStringList urls = m_adBlocker->blockedUrlsForHost(host);
    if (urls.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("拦截详情"),
            QStringLiteral("当前站点尚未拦截任何请求。"));
        return;
    }
    QMessageBox::information(this, QStringLiteral("拦截详情"),
        QStringLiteral("已在 %1 拦截 %2 条请求：\n\n%3")
            .arg(host)
            .arg(urls.size())
            .arg(urls.mid(0, 30).join(QLatin1Char('\n'))));
}

void BrowserWindow::clearBrowsingData()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("清除浏览数据"));
    auto *v = new QVBoxLayout(&dlg);

    auto *cbHistory = new QCheckBox(QStringLiteral("历史记录"), &dlg);
    auto *cbCache   = new QCheckBox(QStringLiteral("缓存"), &dlg);
    auto *cbCookies = new QCheckBox(QStringLiteral("Cookie 与站点数据"), &dlg);
    auto *cbDown    = new QCheckBox(QStringLiteral("下载记录"), &dlg);
    cbHistory->setChecked(true);
    cbCache->setChecked(true);
    v->addWidget(cbHistory);
    v->addWidget(cbCache);
    v->addWidget(cbCookies);
    v->addWidget(cbDown);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    v->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    if (cbHistory->isChecked()) {
        m_history.clear();
        saveHistory();
        if (m_historyDialog)
            m_historyDialog->setEntries(m_history);
    }
    if (cbCache->isChecked()) {
        auto *profile = QWebEngineProfile::defaultProfile();
        profile->clearHttpCache();
        profile->clearAllVisitedLinks();
    }
    if (cbCookies->isChecked()) {
        QWebEngineProfile::defaultProfile()->cookieStore()->deleteAllCookies();
    }
    if (cbDown->isChecked()) {
        const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/downloads.json");
        QFile::remove(path);
    }

    statusBar()->showMessage(QStringLiteral("已清除所选浏览数据"), 3000);
}

void BrowserWindow::showSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}


QByteArray BrowserWindow::mergeSyncData(const QByteArray &local, const QByteArray &remote)
{
    return SyncMerge::merge(local, remote);
}

QByteArray BrowserWindow::exportSyncData() const
{
    QJsonObject root;

    QJsonArray bmArr;
    for (const Bookmark &b : m_bookmarks) {
        QJsonObject o;
        o.insert(QStringLiteral("title"), b.title);
        o.insert(QStringLiteral("url"), b.url.toString());
        o.insert(QStringLiteral("group"), b.group);
        bmArr.append(o);
    }
    root.insert(QStringLiteral("bookmarks"), bmArr);

    QJsonArray hisArr;
    for (const HistoryEntry &e : m_history) {
        QJsonObject o;
        o.insert(QStringLiteral("title"), e.title);
        o.insert(QStringLiteral("url"), e.url.toString());
        o.insert(QStringLiteral("time"), e.visitedAt.toString(Qt::ISODate));
        hisArr.append(o);
    }
    root.insert(QStringLiteral("history"), hisArr);

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool BrowserWindow::importSyncData(const QByteArray &data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return false;
    const QJsonObject root = doc.object();

    // 书签
    m_bookmarks.clear();
    for (const QJsonValue &v : root.value(QStringLiteral("bookmarks")).toArray()) {
        const QJsonObject o = v.toObject();
        Bookmark b;
        b.title = o.value(QStringLiteral("title")).toString();
        b.url   = QUrl(o.value(QStringLiteral("url")).toString());
        b.group = o.value(QStringLiteral("group")).toString();
        if (b.url.isValid())
            m_bookmarks.append(b);
    }

    // 历史
    m_history.clear();
    for (const QJsonValue &v : root.value(QStringLiteral("history")).toArray()) {
        const QJsonObject o = v.toObject();
        HistoryEntry e;
        e.title = o.value(QStringLiteral("title")).toString();
        e.url   = QUrl(o.value(QStringLiteral("url")).toString());
        e.visitedAt = QDateTime::fromString(
            o.value(QStringLiteral("time")).toString(), Qt::ISODate);
        if (e.url.isValid())
            m_history.append(e);
    }

    saveBookmarks();
    saveHistory();
    rebuildBookmarkBar();
    if (m_historyDialog)
        m_historyDialog->setEntries(m_history);
    return true;
}

void BrowserWindow::showSyncDialog()
{
    SyncDialog dlg(this);
    dlg.exec();
}

void BrowserWindow::toggleAdBlock(bool enabled)
{
    if (m_adBlocker)
        m_adBlocker->setEnabled(enabled);
    statusBar()->showMessage(
        enabled ? QStringLiteral("广告拦截已启用")
                : QStringLiteral("广告拦截已关闭"), 2000);
}


void BrowserWindow::showUserScriptManager()
{
    UserScriptManager dlg(this);
    dlg.exec();
}

void BrowserWindow::showToolbox()
{
    ToolboxDialog dlg(this);
    dlg.exec();
}

void BrowserWindow::toggleAiSidebar()
{
    if (!m_aiSidebar) {
        m_aiSidebar = new AiSidebar(this);
        addDockWidget(Qt::RightDockWidgetArea, m_aiSidebar);
    }
    m_aiSidebar->setVisible(!m_aiSidebar->isVisible());

    if (m_aiSidebar->isVisible()) {
        // 带入当前页正文作为上下文
        auto *v = currentView();
        if (v) {
            v->page()->toPlainText([this, v](const QString &text) {
                if (m_aiSidebar)
                    m_aiSidebar->setPageContext(v->title(), text.left(8000));
            });
        }
    }
}

void BrowserWindow::toggleBookmarkSidebar()
{
    if (!m_bookmarkSidebar) {
        m_bookmarkSidebar = new BookmarkSidebar(this);
        addDockWidget(Qt::LeftDockWidgetArea, m_bookmarkSidebar);
        connect(m_bookmarkSidebar, &BookmarkSidebar::urlActivated, this,
                [this](const QUrl &url) { createTab(url, true); });
    }
    const bool show = !m_bookmarkSidebar->isVisible();
    m_bookmarkSidebar->setVisible(show);

    if (show) {
        QList<QPair<QString, QUrl>> bms;
        for (const Bookmark &b : m_bookmarks)
            bms.append({b.title, b.url});
        m_bookmarkSidebar->setBookmarks(bms);

        QList<QPair<QString, QUrl>> his;
        for (const HistoryEntry &e : m_history)
            his.append({e.title, e.url});
        m_bookmarkSidebar->setHistory(his);
    }
}

void BrowserWindow::showAiChat()
{
    AiDialog dlg(this);
    dlg.exec();
}

void BrowserWindow::aiSummarizePage()
{
    WebView *v = currentView();
    if (!v) {
        QMessageBox::information(this, QStringLiteral("AI"),
                                 QStringLiteral("没有可总结的页面。"));
        return;
    }

    // 提取正文
    const QString script = QStringLiteral(
        "(function(){"
        "var t=document.body?document.body.innerText:'';"
        "t=t.replace(/\\s+/g,' ').trim();"
        "return t.slice(0,8000);"
        "})();");

    v->page()->runJavaScript(script, [this](const QVariant &result) {
        const QString text = result.toString().trimmed();
        if (text.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("AI"),
                                     QStringLiteral("页面没有可提取的正文。"));
            return;
        }
        AiDialog dlg(this);
        dlg.askWithPrompt(
            QStringLiteral("请用简体中文总结以下网页内容，先给一句话概述，再列关键要点：\n\n")
            + text);
        dlg.exec();
    });
}

void BrowserWindow::aiTranslatePage()
{
    WebView *v = currentView();
    if (!v) {
        QMessageBox::information(this, QStringLiteral("AI"),
                                 QStringLiteral("没有可翻译的页面。"));
        return;
    }

    // 提取正文
    const QString script = QStringLiteral(
        "(function(){"
        "var t=document.body?document.body.innerText:'';"
        "t=t.replace(/\\s+/g,' ').trim();"
        "return t.slice(0,8000);"
        "})();");

    v->page()->runJavaScript(script, [this](const QVariant &result) {
        const QString text = result.toString().trimmed();
        if (text.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("AI"),
                                     QStringLiteral("页面没有可提取的正文。"));
            return;
        }
        AiDialog dlg(this);
        dlg.askWithPrompt(
            QStringLiteral("请把以下网页内容翻译成简体中文（若原文已是中文则翻译成英文）。"
                           "保留段落结构，直接输出译文：\n\n")
            + text);
        dlg.exec();
    });
}


namespace {
// 为带 GM_* 授权的脚本生成 localStorage 垫片
static QString gmShim(const UserScript &s)
{
    if (s.grants.isEmpty())
        return QString();
    bool needStorage = false;
    for (const QString &g : s.grants) {
        if (g.startsWith(QStringLiteral("GM_")) && g.contains(QStringLiteral("Value")))
            needStorage = true;
    }
    if (!needStorage)
        return QString();
    return QStringLiteral(
        "(function(){"
        "window.GM_setValue=function(k,v){try{localStorage.setItem('__breeze_gm_'+k,JSON.stringify(v));}catch(e){}};"
        "window.GM_getValue=function(k,d){try{var v=localStorage.getItem('__breeze_gm_'+k);"
        "return v===null?d:JSON.parse(v);}catch(e){return d;}};"
        "window.GM_deleteValue=function(k){try{localStorage.removeItem('__breeze_gm_'+k);}catch(e){}};"
        "})();");
}
}

void BrowserWindow::injectStartScripts(WebView *view, const QUrl &url)
{
    if (!view || !view->page())
        return;

    const QList<UserScript> scripts = UserScriptManager::loadScripts();
    QWebEngineScriptCollection &collection = view->page()->scripts();

    int idx = 0;
    for (const UserScript &s : scripts) {
        if (!s.enabled || s.isCss)
            continue;
        if (s.runAt != QStringLiteral("document-start"))
            continue;
        if (!UserScriptManager::matchesUrl(s, url))
            continue;

        QWebEngineScript qs;
        qs.setName(QStringLiteral("breeze-start-%1-%2").arg(idx++).arg(s.name));
        qs.setSourceCode(gmShim(s) + s.code);
        qs.setInjectionPoint(QWebEngineScript::DocumentCreation);
        qs.setWorldId(QWebEngineScript::MainWorld);
        qs.setRunsOnSubFrames(false);
        collection.insert(qs);
    }
}

void BrowserWindow::injectExtensionScripts(WebView *view, const QUrl &url)
{
    if (!view || !view->page())
        return;

    const QList<Extension> exts = ExtensionManager::loadAll();
    QWebEngineScriptCollection &collection = view->page()->scripts();

    for (const Extension &ext : exts) {
        for (const ContentScript &cs : ext.contentScripts) {
            if (!ExtensionManager::matchesUrl(cs, url))
                continue;

            // 拼接 JS 内容
            QString code;
            for (const QString &rel : cs.js) {
                QFile f(ext.dir + QLatin1Char('/') + rel);
                if (f.open(QIODevice::ReadOnly))
                    code += QString::fromUtf8(f.readAll()) + QLatin1Char('\n');
            }
            // CSS 包成 <style> 注入
            for (const QString &rel : cs.css) {
                QFile f(ext.dir + QLatin1Char('/') + rel);
                if (f.open(QIODevice::ReadOnly)) {
                    const QString css = QString::fromUtf8(f.readAll());
                    const QString cssJson = QString::fromUtf8(
                        QJsonDocument(QJsonArray{ css }).toJson(QJsonDocument::Compact));
                    const QString cssLiteral = cssJson.mid(1, cssJson.length() - 2);
                    code += QStringLiteral(
                        "(function(){var st=document.createElement('style');"
                        "st.textContent=%1;document.head.appendChild(st);})();\n")
                        .arg(cssLiteral);
                }
            }

            if (code.trimmed().isEmpty())
                continue;

            QWebEngineScript qs;
            qs.setName(QStringLiteral("breeze-ext-%1").arg(ext.name));
            qs.setSourceCode(code);
            if (cs.runAt == QStringLiteral("document_start"))
                qs.setInjectionPoint(QWebEngineScript::DocumentCreation);
            else if (cs.runAt == QStringLiteral("document_end"))
                qs.setInjectionPoint(QWebEngineScript::DocumentReady);
            else
                qs.setInjectionPoint(QWebEngineScript::Deferred);
            qs.setWorldId(QWebEngineScript::MainWorld);
            qs.setRunsOnSubFrames(false);
            collection.insert(qs);
        }
    }
}

void BrowserWindow::injectUserScripts(WebView *view)
{
    if (!view)
        return;
    const QList<UserScript> scripts = UserScriptManager::loadScripts();
    for (const UserScript &s : scripts) {
        if (!s.enabled)
            continue;
        if (!UserScriptManager::matchesUrl(s, view->url()))
            continue;
        // document-start 脚本已在加载前通过 QWebEngineScript 注册，跳过
        if (!s.isCss && s.runAt == QStringLiteral("document-start"))
            continue;

        if (s.isCss) {
            // 把 CSS 包成 <style> 注入；用 JSON 字符串安全转义
            const QString cssJson = QString::fromUtf8(
                QJsonDocument(QJsonArray{ s.code }).toJson(QJsonDocument::Compact));
            // cssJson 形如 ["..."]，去掉外层方括号得到 JSON 字符串
            const QString cssLiteral = cssJson.mid(1, cssJson.length() - 2);
            const QString js =
                QStringLiteral("(function(){var st=document.createElement('style');"
                               "st.textContent=%1;document.head.appendChild(st);})();")
                    .arg(cssLiteral);
            view->page()->runJavaScript(js);
        } else {
            const QString shim = gmShim(s);
            view->page()->runJavaScript(shim + s.code);
        }
    }
}



void BrowserWindow::onLoadStarted()
{
    if (sender() != currentView())
        return;
    m_progress->setValue(0);
    m_progress->show();
    // 极简模式无停止按钮
}

void BrowserWindow::onLoadProgress(int progress)
{
    if (sender() != currentView())
        return;
    m_progress->setValue(progress);
}

void BrowserWindow::onLoadFinished(bool ok)
{
    Q_UNUSED(ok);
    // 极简模式无停止按钮

    // 仅当加载完成的是当前标签时，才更新进度条与导航按钮
    auto *view = qobject_cast<WebView *>(sender());
    if (view && view == currentView()) {
        m_progress->hide();
        m_progress->setValue(0);
        updateNavButtons();
    }

    // 记录历史：使用实际完成加载的标签，而非当前标签
    recordHistory(view);

    // 注入匹配的用户脚本
    if (ok && view)
        injectUserScripts(view);

    // 安装悬停链接监听（状态栏显示目标地址）
    if (view)
        view->installHoverWatcher();

    // 安装划词工具栏
    if (view)
        view->installSelectionToolbar();

    // 按域名恢复页面缩放
    if (view)
        applySavedZoom(view);
}

void BrowserWindow::navBack()
{
    if (auto *v = currentView()) v->back();
}

void BrowserWindow::navForward()
{
    if (auto *v = currentView()) v->forward();
}

void BrowserWindow::navReload()
{
    if (auto *v = currentView()) v->reload();
}

void BrowserWindow::navStop()
{
    if (auto *v = currentView()) v->stop();
}


void BrowserWindow::saveSession() const
{
    if (!m_tabs)
        return;
    QStringList urls;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *v = qobject_cast<WebView *>(m_tabs->widget(i));
        if (!v || v->property("breezePrivate").toBool())
            continue;
        const QUrl u = v->url();
        if (u.isValid() && !u.isEmpty())
            urls << u.toString();
    }
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("session/urls"), urls);
    s.setValue(QStringLiteral("session/current"), m_tabs->currentIndex());
}

void BrowserWindow::restoreSession()
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    const QStringList urls = s.value(QStringLiteral("session/urls")).toStringList();

    if (urls.isEmpty()) {
        createTab(homeUrl());
        return;
    }

    for (const QString &u : urls)
        createTab(QUrl(u), false);   // 先都不切换，最后统一切

    const int idx = s.value(QStringLiteral("session/current"), 0).toInt();
    if (idx >= 0 && idx < m_tabs->count())
        m_tabs->setCurrentIndex(idx);
    else if (m_tabs->count() > 0)
        m_tabs->setCurrentIndex(0);
}

void BrowserWindow::saveNamedSession()
{
    if (!m_tabs)
        return;
    QStringList urls;
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *v = qobject_cast<WebView *>(m_tabs->widget(i));
        if (!v || v->property("breezePrivate").toBool())
            continue;
        const QUrl u = v->url();
        if (u.isValid() && !u.isEmpty())
            urls << u.toString();
    }
    if (urls.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("保存会话"),
                                 QStringLiteral("当前没有可保存的标签页。"));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("保存会话"), QStringLiteral("会话名称："),
        QLineEdit::Normal, QStringLiteral("会话 %1").arg(QDate::currentDate().toString(QStringLiteral("MM-dd"))),
        &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.beginGroup(QStringLiteral("namedSessions"));
    s.setValue(name + QStringLiteral("/urls"), urls);
    s.endGroup();

    statusBar()->showMessage(QStringLiteral("会话已保存：%1（%2 个标签）")
                                 .arg(name).arg(urls.size()), 3000);
}

void BrowserWindow::manageSessions()
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.beginGroup(QStringLiteral("namedSessions"));
    const QStringList names = s.childGroups();
    s.endGroup();

    if (names.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("会话管理"),
                                 QStringLiteral("还没有保存的会话。"));
        return;
    }

    // 简单列表对话框：选中恢复 / 删除
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("会话管理"));
    dlg.resize(420, 360);
    auto *layout = new QVBoxLayout(&dlg);
    auto *list = new QListWidget(&dlg);
    layout->addWidget(list, 1);
    auto *btnRow = new QHBoxLayout;
    auto *openBtn = new QPushButton(QStringLiteral("恢复"), &dlg);
    auto *delBtn  = new QPushButton(QStringLiteral("删除"), &dlg);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
    btnRow->addWidget(openBtn);
    btnRow->addWidget(delBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    for (const QString &n : names)
        list->addItem(n);

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    connect(delBtn, &QPushButton::clicked, &dlg, [this, list, &names]() {
        auto *item = list->currentItem();
        if (!item)
            return;
        const QString name = item->text();
        if (QMessageBox::question(this, QStringLiteral("删除会话"),
                QStringLiteral("删除会话「%1」？").arg(name)) != QMessageBox::Yes)
            return;
        QSettings s2(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
        s2.beginGroup(QStringLiteral("namedSessions"));
        s2.remove(name);
        s2.endGroup();
        delete item;
    });

    connect(openBtn, &QPushButton::clicked, &dlg, [this, list, &dlg]() {
        auto *item = list->currentItem();
        if (!item)
            return;
        const QString name = item->text();
        QSettings s2(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
        s2.beginGroup(QStringLiteral("namedSessions"));
        const QStringList urls = s2.value(name + QStringLiteral("/urls")).toStringList();
        s2.endGroup();

        for (const QString &u : urls)
            createTab(QUrl(u), false);
        if (!urls.isEmpty())
            m_tabs->setCurrentIndex(m_tabs->count() - urls.size());

        dlg.accept();
    });

    dlg.exec();
}

void BrowserWindow::printPage()
{
    auto *v = currentView();
    if (!v)
        return;

    // Qt6 WebEngine 只提供 printToPdf；先导出 PDF 到临时文件，
    // 完成后用系统默认 PDF 阅读器打开（用户可在其中打印）。
    const QString tmp = QDir::tempPath()
                        + QStringLiteral("/breeze_print_")
                        + QString::number(QDateTime::currentMSecsSinceEpoch())
                        + QStringLiteral(".pdf");

    statusBar()->showMessage(QStringLiteral("正在生成打印文件…"), 2000);

    auto *page = v->page();
    connect(page, &QWebEnginePage::pdfPrintingFinished, this,
            [this](const QString &path, bool ok) {
                if (ok) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                    statusBar()->showMessage(QStringLiteral("已生成打印文件"), 3000);
                } else {
                    statusBar()->showMessage(QStringLiteral("生成打印文件失败"), 3000);
                }
            },
            Qt::SingleShotConnection);

    page->printToPdf(tmp);
}

void BrowserWindow::savePageAsPdf()
{
    auto *v = currentView();
    if (!v)
        return;

    const QString name = v->title().isEmpty() ? QStringLiteral("page") : v->title();
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存为 PDF"),
        dir + QLatin1Char('/') + name + QStringLiteral(".pdf"),
        QStringLiteral("PDF 文件 (*.pdf)"));
    if (path.isEmpty())
        return;

    v->page()->printToPdf(path);
    statusBar()->showMessage(QStringLiteral("已导出 PDF：%1").arg(path), 3000);
}

void BrowserWindow::savePageAsHtml()
{
    auto *v = currentView();
    if (!v)
        return;

    const QString name = v->title().isEmpty() ? QStringLiteral("page") : v->title();
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存为 MHTML"),
        dir + QLatin1Char('/') + name + QStringLiteral(".mhtml"),
        QStringLiteral("MHTML 文件 (*.mhtml)"));
    if (path.isEmpty())
        return;

    const QString filePath = path;
    v->page()->save(filePath, QWebEngineDownloadRequest::MimeHtmlSaveFormat);
    statusBar()->showMessage(QStringLiteral("正在保存：%1").arg(filePath), 3000);
}


void BrowserWindow::capturePage()
{
    auto *v = currentView();
    if (!v)
        return;

    const QPixmap shot = v->grab();
    if (shot.isNull()) {
        QMessageBox::warning(this, QStringLiteral("截图失败"),
                             QStringLiteral("无法捕获当前页面。"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存截图"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
            + QStringLiteral("/breeze-shot.png"),
        QStringLiteral("PNG 图片 (*.png)"));
    if (path.isEmpty())
        return;

    if (shot.save(path, "PNG"))
        statusBar()->showMessage(QStringLiteral("截图已保存：%1").arg(path), 3000);
    else
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("无法写入图片文件。"));
}

void BrowserWindow::captureFullPage()
{
    auto *v = currentView();
    if (!v)
        return;

    // 先询问保存路径
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存整页截图"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
            + QStringLiteral("/breeze-fullpage.png"),
        QStringLiteral("PNG 图片 (*.png)"));
    if (path.isEmpty())
        return;

    // 注入 JS 获取页面总高与视口高
    const QString js =
        QStringLiteral("JSON.stringify({h: Math.max(document.body.scrollHeight,"
                       "document.documentElement.scrollHeight),"
                       "v: window.innerHeight})");

    v->page()->runJavaScript(js, [this, v, path](const QVariant &res) {
        const QJsonObject o = QJsonDocument::fromJson(res.toString().toUtf8()).object();
        const int totalH = o.value(QStringLiteral("h")).toInt();
        const int viewH  = o.value(QStringLiteral("v")).toInt();
        if (totalH <= 0 || viewH <= 0) {
            QMessageBox::warning(this, QStringLiteral("截图失败"),
                                 QStringLiteral("无法获取页面尺寸。"));
            return;
        }

        // 记录原滚动位置，完成后恢复
        v->page()->runJavaScript(QStringLiteral("window.scrollY"),
            [this, v, path, totalH, viewH](const QVariant &sy) {
            const int origY = sy.toInt();
            auto frames = std::make_shared<QList<QPair<int, QPixmap>>>();
            auto step = std::make_shared<std::function<void(int)>>();

            *step = [this, v, path, totalH, viewH, origY, frames, step](int y) {
                if (y >= totalH) {
                    // 全部抓完，拼接
                    int width = 0, height = 0;
                    for (const auto &p : *frames) {
                        width = qMax(width, p.second.width());
                        height += p.second.height();
                    }
                    if (width == 0 || height == 0) {
                        QMessageBox::warning(this, QStringLiteral("截图失败"),
                                             QStringLiteral("未捕获到画面。"));
                        v->page()->runJavaScript(QStringLiteral("window.scrollTo(0,%1)").arg(origY));
                        return;
                    }
                    QPixmap result(width, height);
                    result.fill(Qt::white);
                    QPainter painter(&result);
                    int dy = 0;
                    for (const auto &p : *frames) {
                        painter.drawPixmap(0, dy, p.second);
                        dy += p.second.height();
                    }
                    painter.end();

                    if (result.save(path, "PNG"))
                        statusBar()->showMessage(
                            QStringLiteral("整页截图已保存：%1").arg(path), 3000);
                    else
                        QMessageBox::warning(this, QStringLiteral("保存失败"),
                                             QStringLiteral("无法写入图片文件。"));
                    v->page()->runJavaScript(QStringLiteral("window.scrollTo(0,%1)").arg(origY));
                    return;
                }

                v->page()->runJavaScript(QStringLiteral("window.scrollTo(0,%1)").arg(y));
                QTimer::singleShot(350, this, [this, v, y, frames, step]() {
                    frames->append({y, v->grab()});
                    (*step)(y + v->height());
                });
            };

            (*step)(0);
        });
    });
}

void BrowserWindow::zoomIn()
{
    applyZoom(0.1);
}

void BrowserWindow::zoomOut()
{
    applyZoom(-0.1);
}

void BrowserWindow::zoomReset()
{
    if (auto *v = currentView()) {
        v->setZoomFactor(1.0);
        saveZoomForView(v);
    }
}

void BrowserWindow::applyZoom(double delta)
{
    auto *v = currentView();
    if (!v)
        return;
    double f = v->zoomFactor() + delta;
    f = qBound(0.25, f, 5.0);
    v->setZoomFactor(f);
    saveZoomForView(v);
    statusBar()->showMessage(
        QStringLiteral("缩放：%1%").arg(qRound(f * 100)), 1500);
}

// 记录某标签当前页面的缩放比例（按域名）。隐私标签不持久化。
void BrowserWindow::saveZoomForView(WebView *view)
{
    if (!view || view->property("breezePrivate").toBool())
        return;
    const QString host = view->url().host();
    if (host.isEmpty())
        return;
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("zoom/") + host, view->zoomFactor());
}

// 按域名恢复缩放比例（页面加载完成后调用）。隐私标签不恢复。
void BrowserWindow::applySavedZoom(WebView *view)
{
    if (!view || view->property("breezePrivate").toBool())
        return;
    const QString host = view->url().host();
    if (host.isEmpty())
        return;
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    const double f = s.value(QStringLiteral("zoom/") + host, 1.0).toDouble();
    if (f > 0.0 && !qFuzzyCompare(f, 1.0))
        view->setZoomFactor(qBound(0.25, f, 5.0));
}


void BrowserWindow::showFindBar()
{
    m_findBar->show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

void BrowserWindow::hideFindBar()
{
    if (auto *v = currentView())
        v->findText(QString());   // 清除高亮
    if (m_findCountLabel)
        m_findCountLabel->clear();
    m_findBar->hide();
    if (auto *v = currentView())
        v->setFocus();
}

void BrowserWindow::findNext()
{
    if (auto *v = currentView()) {
        const QString t = m_findEdit->text();
        if (!t.isEmpty())
            v->findText(t, QWebEnginePage::FindFlags());
    }
}

void BrowserWindow::findPrevious()
{
    if (auto *v = currentView()) {
        const QString t = m_findEdit->text();
        if (!t.isEmpty())
            v->findText(t, QWebEnginePage::FindBackward);
    }
}


void BrowserWindow::navHome()
{
    if (auto *v = currentView())
        v->setUrl(homeUrl());
}

void BrowserWindow::updateTabTitle(WebView *view)
{
    const int index = m_tabs->indexOf(view);
    if (index < 0)
        return;

    QString title = view->title();
    if (title.isEmpty()) {
        const QUrl u = view->url();
        title = u.isValid() && !u.isEmpty() ? u.host() : QStringLiteral("新标签页");
    }
    if (title.size() > 24)
        title = title.left(24) + QStringLiteral("\u2026");
    if (m_pinnedTabs.contains(view))
        title = QStringLiteral("📌 ") + title;
    m_tabs->setTabText(index, title);
    m_tabs->setTabToolTip(index, view->title());

    if (view == currentView()) {
        setWindowTitle(view->title().isEmpty()
                           ? QStringLiteral("Breeze ") + QStringLiteral(BREEZE_VERSION)
                           : QStringLiteral("%1 - Breeze ").arg(view->title())
                                 + QStringLiteral(BREEZE_VERSION));
    }
}

void BrowserWindow::updateTabUrl(WebView *view, const QUrl &url)
{
    if (view == currentView()) {
        m_urlBar->setText(url.toString());
        m_urlBar->setCursorPosition(0);
        updateNavButtons();

        // 安全状态指示
        if (m_securityLabel) {
            const QString scheme = url.scheme();
            if (scheme == QStringLiteral("https")) {
                m_securityLabel->setText(QStringLiteral("🔒"));
                m_securityLabel->setToolTip(QStringLiteral("安全连接（HTTPS）"));
            } else if (scheme == QStringLiteral("http")) {
                m_securityLabel->setText(QStringLiteral("⚠"));
                m_securityLabel->setToolTip(QStringLiteral("不安全连接（HTTP）"));
            } else {
                m_securityLabel->clear();
                m_securityLabel->setToolTip(QString());
            }
        }
    }
}

void BrowserWindow::updateNavButtons()
{
    WebView *v = currentView();
    if (!v)
        return;

    if (auto *h = v->history()) {
        m_actBack->setEnabled(h->canGoBack());
        m_actForward->setEnabled(h->canGoForward());
    }
}
