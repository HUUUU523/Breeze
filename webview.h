#ifndef WEBVIEW_H
#define WEBVIEW_H

#include <QWebEngineView>
#include <functional>

class QWebEngineProfile;
class AdBlocker;

// 自定义 WebView：
// - 处理 window.open / target=_blank 新窗口请求
// - 增强右键菜单（AI 处理选中文字、复制链接等）
class WebView : public QWebEngineView
{
    Q_OBJECT

public:
    explicit WebView(QWidget *parent = nullptr);
    WebView(QWebEngineProfile *profile, QWidget *parent);

    // 设置"提供新标签 WebView"的回调（由主窗口注入）
    void setNewTabProvider(std::function<WebView *()> provider);

    // 设置广告拦截器
    void setAdBlocker(AdBlocker *blocker);

signals:
    // 请求对选中文字执行 AI 操作（action: explain / translate / rewrite）
    void aiActionRequested(const QString &action, const QString &text);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    std::function<WebView *()> m_newTabProvider;
};

#endif // WEBVIEW_H
