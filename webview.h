#ifndef WEBVIEW_H
#define WEBVIEW_H

#include <QWebEngineView>
#include <functional>

class QWebEngineProfile;

// 自定义 WebView：
// - 处理 window.open / target=_blank 新窗口请求：
//   通过 newTabProvider 回调向主窗口索取一个"已加入标签栏的新 WebView"，
//   并由 Qt 将新页面加载进该视图，从而真正打开新标签。
class WebView : public QWebEngineView
{
    Q_OBJECT

public:
    explicit WebView(QWidget *parent = nullptr);

    // 使用指定 profile 构造（隐私模式传独立的临时 profile）
    WebView(QWebEngineProfile *profile, QWidget *parent);

    // 设置"提供新标签 WebView"的回调（由主窗口注入）
    void setNewTabProvider(std::function<WebView *()> provider);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

private:
    std::function<WebView *()> m_newTabProvider;
};

#endif // WEBVIEW_H
