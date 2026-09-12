#include "webview.h"

#include <QWebEnginePage>
#include <QWebEngineProfile>

WebView::WebView(QWidget *parent)
    : QWebEngineView(parent)
{
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

WebView::WebView(QWebEngineProfile *profile, QWidget *parent)
    : QWebEngineView(parent)
{
    setContextMenuPolicy(Qt::DefaultContextMenu);
    if (profile) {
        // 为该视图创建一个使用指定 profile 的 page
        setPage(new QWebEnginePage(profile, this));
    }
}

void WebView::setNewTabProvider(std::function<WebView *()> provider)
{
    m_newTabProvider = std::move(provider);
}

QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type);
    // 由主窗口创建一个已加入标签栏的空 WebView，Qt 会把新窗口内容加载进去
    if (m_newTabProvider)
        return m_newTabProvider();
    return nullptr;
}
