#include "webview.h"
#include "adblocker.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>

// ===================== BreezeWebPage =====================

namespace {
const char *kHoverPrefix = "__BREEZE_HOVER__:";
}

BreezeWebPage::BreezeWebPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
}

BreezeWebPage::BreezeWebPage(QObject *parent)
    : QWebEnginePage(parent)
{
}

void BreezeWebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                             const QString &message, int lineNumber,
                                             const QString &sourceID)
{
    Q_UNUSED(level);
    Q_UNUSED(lineNumber);
    Q_UNUSED(sourceID);

    if (message.startsWith(QLatin1String(kHoverPrefix))) {
        emit hoverUrlChanged(message.mid(int(qstrlen(kHoverPrefix))));
        return;
    }
    QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceID);
}

// ===================== WebView =====================

WebView::WebView(QWidget *parent)
    : QWebEngineView(parent)
{
    setPage(new BreezeWebPage(this));
    connect(qobject_cast<BreezeWebPage *>(page()), &BreezeWebPage::hoverUrlChanged,
            this, &WebView::hoverUrlChanged);
}

WebView::WebView(QWebEngineProfile *profile, QWidget *parent)
    : QWebEngineView(parent)
{
    if (profile) {
        setPage(new BreezeWebPage(profile, this));
        connect(qobject_cast<BreezeWebPage *>(page()), &BreezeWebPage::hoverUrlChanged,
                this, &WebView::hoverUrlChanged);
    }
}

void WebView::setNewTabProvider(std::function<WebView *(bool background)> provider)
{
    m_newTabProvider = std::move(provider);
}

void WebView::setAdBlocker(AdBlocker *blocker)
{
    if (blocker && page())
        page()->setUrlRequestInterceptor(blocker);
}

void WebView::installHoverWatcher()
{
    // 页面里监听 mouseover/mouseout，把 <a href> 通过 console.log 前缀回传。
    // BreezeWebPage::javaScriptConsoleMessage 会解析该前缀。
    const QString js = QStringLiteral(R"JS(
(function(){
  if (window.__breezeHoverWatcher) return;
  window.__breezeHoverWatcher = true;
  var last = null;
  function report(u){ if (u !== last) { last = u; console.log('__BREEZE_HOVER__:' + (u||'')); } }
  document.addEventListener('mouseover', function(e){
    var el = e.target;
    while (el && el.tagName !== 'A') el = el.parentElement;
    report(el ? el.href : '');
  }, true);
  document.addEventListener('mouseout', function(e){
    var el = e.target;
    while (el && el.tagName !== 'A') el = el.parentElement;
    if (el) report('');
  }, true);
})();
)JS");
    page()->runJavaScript(js);
}

QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType type)
{
    if (!m_newTabProvider)
        return nullptr;
    // 中键 / Ctrl+点击 链接通常走 WebBrowserBackgroundTab —— 后台打开
    const bool background = (type == QWebEnginePage::WebBrowserBackgroundTab);
    return m_newTabProvider(background);
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
                QMenu *searchMenu = menu.addMenu(QStringLiteral("搜索选中文字"));
                for (const QString &engine : {QStringLiteral("Bing"),
                                              QStringLiteral("Google"),
                                              QStringLiteral("百度"),
                                              QStringLiteral("DuckDuckGo")}) {
                    connect(searchMenu->addAction(engine), &QAction::triggered, this,
                            [this, engine, t = selectedText]() {
                                emit searchRequested(engine, t);
                            });
                }

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
