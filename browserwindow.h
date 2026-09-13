#ifndef BROWSERWINDOW_H
#define BROWSERWINDOW_H

#include <QByteArray>
#include <QList>
#include <QMainWindow>
#include <QUrl>

#include "historymanager.h"

class QCloseEvent;
class QLineEdit;
class QCompleter;
class QStringListModel;
class QProgressBar;
class QTabWidget;
class QToolBar;
class QAction;
class QMenu;
class QLabel;
class QWebEngineProfile;
class WebView;
class AdBlocker;
class DownloadManager;
class HistoryDialog;

// 单条书签
struct Bookmark {
    QString title;
    QUrl    url;
    QString group;   // 空表示未分组
};

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(QWidget *parent = nullptr);
    ~BrowserWindow() override;

    // 云同步：导出 / 导入本机数据
    QByteArray exportSyncData() const;
    bool importSyncData(const QByteArray &data);

    // 合并两份同步数据（书签/历史取并集，返回合并结果）
    static QByteArray mergeSyncData(const QByteArray &local, const QByteArray &remote);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // ---- 标签页 ----
    void onNewTab();
    void onNewPrivateTab();
    void onCloseTab(int index);
    void onTabChanged(int index);
    void onReopenClosedTab();              // Ctrl+Shift+T
    void onTabBarContextMenu(const QPoint &pos);  // 右键标签：静音等
    void showBlockedDetails();             // 盾牌点击：查看已拦截列表

    // ---- 导航 ----
    void onUrlEntered();
    void onLoadStarted();
    void onLoadProgress(int progress);
    void onLoadFinished(bool ok);
    void navBack();
    void navForward();
    void navReload();
    void navHome();
    void navStop();

    // ---- 书签 ----
    void addBookmarkForCurrentPage();
    void showBookmarkManager();
    void openBookmark(const QUrl &url);
    void removeBookmark(const QUrl &url);
    void rebuildBookmarkBar();
    void addBookmarkAction(const Bookmark &b, QToolBar *bar);

    // ---- 下载 / 历史 ----
    void setupDownloads();
    void showDownloads();
    void setupHistory();
    void showHistory();
    void recordHistory(WebView *view = nullptr);

    // ---- 页面 ----
    void capturePage();
    void showFindBar();
    void hideFindBar();
    void findNext();
    void findPrevious();
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void printPage();
    void savePageAsPdf();

    // ---- 会话 / 设置 / 主题 ----
    void saveSession() const;
    void restoreSession();
    void showSettings();
    void applyTheme();
    void setThemeMode(const QString &mode);   // "system" / "light" / "dark"

    // ---- 云同步 ----
    void showSyncDialog();

    // ---- 更新 ----
    void checkForUpdates();

    // ---- 隐私 ----
    void clearBrowsingData();

    // ---- 扩展 / 工具 ----
    void showUserScriptManager();
    void showToolbox();
    void injectUserScripts(WebView *view);
    void showAiChat();
    void aiSummarizePage();
    // ---- 广告拦截 ----
    void toggleAdBlock(bool enabled);

    // 书签导入导出
    void importBookmarks();
    void exportBookmarks();

    // 历史导入导出
    void importHistory();
    void exportHistory();

    // 阅读模式 / 标签栏位置 / 鼠标手势
    void toggleReaderMode();
    void setTabPosition(int pos);   // 0=顶部 1=左 2=右
    void setMouseGesturesEnabled(bool enabled);

private:
    // ---- 初始化 ----
    void setupUi();
    void setupActions();
    void setupBookmarks();
    void migrateLegacyData();   // 首次启动把旧数据迁到 AppData

    // ---- 标签页内部 ----
    WebView *currentView() const;
    WebView *createTabView(bool privateMode = false);
    WebView *createTab(const QUrl &url, bool switchToTab = true);
    void updateTabTitle(WebView *view);
    void updateTabUrl(WebView *view, const QUrl &url);
    void updateNavButtons();

    // ---- 地址栏辅助 ----
    QUrl homeUrl() const;
    QUrl normalizedUrl(const QString &text) const;
    void refreshUrlCompleter(const QString &prefix);
    void refreshShieldForCurrent();

    // ---- 书签持久化 ----
    void loadBookmarks();
    void saveBookmarks() const;
    QString bookmarksFilePath() const;

    // ---- 历史持久化 ----
    void loadHistory();
    void saveHistory() const;
    QString historyFilePath() const;

    // ---- 缩放 / 主题辅助 ----
    void applyZoom(double delta);
    void saveZoomForView(WebView *view);
    void applySavedZoom(WebView *view);
    QString themeMode() const;
    QWebEngineProfile *privateProfile();   // 隐私 Profile，惰性创建

    // ---- UI ----
    QTabWidget   *m_tabs        = nullptr;
    QLineEdit    *m_urlBar      = nullptr;
    QCompleter   *m_completer   = nullptr;
    QProgressBar *m_progress    = nullptr;
    QLabel       *m_shieldLabel = nullptr;
    QToolBar     *m_bookmarkBar = nullptr;
    QToolBar     *m_findBar     = nullptr;
    QLineEdit    *m_findEdit    = nullptr;

    // ---- 导航 Action ----
    QAction *m_actBack    = nullptr;
    QAction *m_actForward = nullptr;
    QAction *m_actReload  = nullptr;
    // m_actStop / m_actHome 已在极简模式中移除
    QAction *m_actNewTab  = nullptr;
    QAction *m_actNewPrivateTab = nullptr;

    // ---- 功能 Action ----
    QAction *m_actBookmark        = nullptr;
    QAction *m_actManageBookmarks = nullptr;
    QAction *m_actShowBookmarkBar = nullptr;
    QAction *m_actDownloads       = nullptr;
    QAction *m_actHistory         = nullptr;
    QAction *m_actSettings        = nullptr;

    // ---- 阅读模式 / 标签栏 / 手势 ----
    QAction *m_actReader          = nullptr;
    QAction *m_actGestures        = nullptr;

    // ---- 主题 Action ----
    QAction *m_actThemeSystem = nullptr;
    QAction *m_actThemeLight  = nullptr;
    QAction *m_actThemeDark   = nullptr;

    // ---- 数据 ----
    QList<Bookmark>     m_bookmarks;
    QList<HistoryEntry> m_history;
    QList<QUrl>         m_closedTabs;   // 最近关闭的标签（栈，上限 20）
    QUrl                m_homeUrl{"https://www.bing.com"};

    // ---- 子系统 ----
    DownloadManager   *m_downloadManager = nullptr;
    class UpdateManager *m_updateManager = nullptr;
    AdBlocker         *m_adBlocker       = nullptr;
    HistoryDialog     *m_historyDialog   = nullptr;
    QWebEngineProfile *m_privateProfile  = nullptr;
};

#endif // BROWSERWINDOW_H
