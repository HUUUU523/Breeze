#ifndef WEBVIEW_H
#define WEBVIEW_H

#include <QWebEngineView>
#include <QWebEnginePage>
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

signals:
    // 请求对选中文字执行 AI 操作（action: explain / translate / rewrite）
    void aiActionRequested(const QString &action, const QString &text);

    // 悬停链接地址变化（空串表示移出链接）
    void hoverUrlChanged(const QString &url);

    // 请求用指定搜索引擎搜索选中文字
    void searchRequested(const QString &engine, const QString &text);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    std::function<WebView *(bool background)> m_newTabProvider;
};

#endif // WEBVIEW_H
