#include "webview.h"
#include "adblocker.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>

WebView::WebView(QWidget *parent)
    : QWebEngineView(parent)
{
}

WebView::WebView(QWebEngineProfile *profile, QWidget *parent)
    : QWebEngineView(parent)
{
    if (profile) {
        setPage(new QWebEnginePage(profile, this));
    }
}

void WebView::setNewTabProvider(std::function<WebView *()> provider)
{
    m_newTabProvider = std::move(provider);
}

void WebView::setAdBlocker(AdBlocker *blocker)
{
    if (blocker && page())
        page()->setUrlRequestInterceptor(blocker);
}

QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type);
    if (m_newTabProvider)
        return m_newTabProvider();
    return nullptr;
}

void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    // 先异步取选中文字，再弹菜单
    page()->runJavaScript(
        QStringLiteral("window.getSelection().toString()"),
        [this, event](const QVariant &result) {
            const QString selectedText = result.toString().trimmed();

            QMenu menu;
            // 标准动作
            QAction *back = menu.addAction(QStringLiteral("后退"));
            back->setEnabled(history()->canGoBack());
            QAction *forward = menu.addAction(QStringLiteral("前进"));
            forward->setEnabled(history()->canGoForward());
            menu.addAction(QStringLiteral("刷新"), this, &QWebEngineView::reload);
            menu.addSeparator();

            if (!selectedText.isEmpty()) {
                QAction *copy = menu.addAction(QStringLiteral("复制"));
                connect(copy, &QAction::triggered, this, [selectedText]() {
                    QApplication::clipboard()->setText(selectedText);
                });
                QMenu *aiMenu = menu.addMenu(QStringLiteral("AI 处理选中文字"));
                const QString t = selectedText;
                connect(aiMenu->addAction(QStringLiteral("解释")), &QAction::triggered, this,
                        [this, t]() { emit aiActionRequested(QStringLiteral("explain"), t); });
                connect(aiMenu->addAction(QStringLiteral("翻译")), &QAction::triggered, this,
                        [this, t]() { emit aiActionRequested(QStringLiteral("translate"), t); });
                connect(aiMenu->addAction(QStringLiteral("改写")), &QAction::triggered, this,
                        [this, t]() { emit aiActionRequested(QStringLiteral("rewrite"), t); });
                menu.addSeparator();
            }

            menu.addAction(QStringLiteral("复制页面地址"), this, [this]() {
                QApplication::clipboard()->setText(url().toString());
            });
            menu.addAction(QStringLiteral("查看源代码"), this, [this]() {
                setUrl(QUrl(QStringLiteral("view-source:") + url().toString()));
            });

            connect(back, &QAction::triggered, this, &QWebEngineView::back);
            connect(forward, &QAction::triggered, this, &QWebEngineView::forward);

            menu.exec(event->globalPos());
        });
}
