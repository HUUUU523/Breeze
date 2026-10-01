#ifndef WEBVIEW_H
#define WEBVIEW_H

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineCertificateError>
#include <functional>

class QWebEngineProfile;
class AdBlocker;

// 自定义 QWebEnginePage：把页面里 console.log('__BREEZE_HOVER__:' + url)
// 解析出来，通过 hoverUrlChanged 信号抛给上层（用于状态栏显示链接地址）。
class BreezeWebPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit BreezeWebPage(QWebEngineProfile *profile, QObject *parent = nullptr);
    explicit BreezeWebPage(QObject *parent = nullptr);

signals:
    void hoverUrlChanged(const QString &url);
    // 划词工具栏动作：action = explain/translate/search，text 为选中文字
    void selectionActionRequested(const QString &action, const QString &text);
    // 扩展消息总线：页面通过 chrome.runtime.sendMessage 发出的消息
    void extMessage(const QString &json);
    // 扩展 tabs 命令（chrome.tabs.remove/reload 等）
    void tabsCommand(const QString &json);
    // 元素选择器返回的矩形（JSON 字符串）
    void elementRectSelected(const QString &json);

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message, int lineNumber,
                                  const QString &sourceID) override;
};

// 自定义 WebView：
// - 处理 window.open / target=_blank 新窗口请求
// - 增强右键菜单（AI 处理选中文字、复制链接等）
// - 悬停链接时把目标地址上报给主窗口
class WebView : public QWebEngineView
{
    Q_OBJECT

public:
    explicit WebView(QWidget *parent = nullptr);
    WebView(QWebEngineProfile *profile, QWidget *parent);

    // 设置"提供新标签 WebView"的回调（由主窗口注入）
    // 参数 background=true 表示新标签应在后台打开（不切换过去）
    void setNewTabProvider(std::function<WebView *(bool background)> provider);

    // 设置广告拦截器
    void setAdBlocker(AdBlocker *blocker);

    // 安装悬停链接监听（注入 JS，经 console.log 前缀回传）
    void installHoverWatcher();

    // 安装划词工具栏（注入 JS）
    void installSelectionToolbar();

    // 是否启用划词工具栏（默认开）
    void setSelectionToolbarEnabled(bool enabled);

signals:
    // 请求对选中文字执行 AI 操作（action: explain / translate / rewrite）
    void aiActionRequested(const QString &action, const QString &text);

    // 悬停链接地址变化（空串表示移出链接）
    void hoverUrlChanged(const QString &url);

    // 请求用指定搜索引擎搜索选中文字
    void searchRequested(const QString &engine, const QString &text);

    // 划词工具栏动作
    void selectionActionRequested(const QString &action, const QString &text);

    // 请求在新标签打开某 URL
    void newTabRequested(const QUrl &url, bool switchToTab);

    // 扩展消息总线：页面通过 chrome.runtime.sendMessage 发出的消息
    void extMessage(const QString &json);
    // 扩展 tabs 命令（chrome.tabs.remove/reload 等）
    void tabsCommand(const QString &json);
    // 鼠标手势：轨迹方向串（如 "LR" 表示先左后右）
    void gestureTriggered(const QString &gesture);
    // 手势进行中的实时轨迹（用于绘制提示）
    void gestureProgress(const QPoint &pos);
    // 元素选择器返回的矩形（JSON 字符串）
    void elementRectSelected(const QString &json);


protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onCertificateError(const QWebEngineCertificateError &error);

private:
    void connectPageSignals();

    std::function<WebView *(bool background)> m_newTabProvider;
    bool m_selectionToolbarEnabled = true;

    // 鼠标手势状态
    bool m_gestureActive = false;
    QPoint m_gestureStart;
    QPoint m_gestureLast;
    QString m_gestureDirs;
};

#endif // WEBVIEW_H

