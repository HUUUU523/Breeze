#include "webview.h"
#include "adblocker.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>
#include <QWebEngineCertificateError>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QMessageBox>

// ===================== BreezeWebPage =====================

namespace {
const char *kHoverPrefix  = "__BREEZE_HOVER__:";
const char *kSelectPrefix = "__BREEZE_SELECT__:";
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
    if (message.startsWith(QLatin1String(kSelectPrefix))) {
        // 格式：__BREEZE_SELECT__:action:text
        const QString rest = message.mid(int(qstrlen(kSelectPrefix)));
        const int sep = rest.indexOf(QLatin1Char(':'));
        if (sep > 0) {
            const QString action = rest.left(sep);
            const QString text = rest.mid(sep + 1);
            emit selectionActionRequested(action, text);
        }
        return;
    }
    QWebEnginePage::javaScriptConsoleMessage(level, message, lineNumber, sourceID);
}

// ===================== WebView =====================

WebView::WebView(QWidget *parent)
    : QWebEngineView(parent)
{
    setPage(new BreezeWebPage(this));
    connectPageSignals();
}

WebView::WebView(QWebEngineProfile *profile, QWidget *parent)
    : QWebEngineView(parent)
{
    if (profile) {
        setPage(new BreezeWebPage(profile, this));
        connectPageSignals();
    }
}

void WebView::connectPageSignals()
{
    auto *bp = qobject_cast<BreezeWebPage *>(page());
    if (!bp)
        return;
    connect(bp, &BreezeWebPage::hoverUrlChanged,
            this, &WebView::hoverUrlChanged);
    connect(bp, &BreezeWebPage::selectionActionRequested,
            this, &WebView::selectionActionRequested);
    connect(bp, &QWebEnginePage::certificateError,
            this, &WebView::onCertificateError);
}

void WebView::onCertificateError(const QWebEngineCertificateError &error)
{
    auto err = error;
    err.defer();   // 延后，由弹框结果决定

    const QString msg = QStringLiteral("无法验证 %1 的安全证书。\n\n原因：%2\n\n是否继续访问？")
        .arg(err.url().host(), err.description());

    // 不可覆盖的错误（如证书吊销）直接拒绝
    if (!err.isOverridable()) {
        QMessageBox::warning(this, QStringLiteral("证书错误"), msg);
        err.rejectCertificate();
        return;
    }

    const auto ret = QMessageBox::warning(
        this, QStringLiteral("证书错误"), msg,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        err.acceptCertificate();
    else
        err.rejectCertificate();
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

void WebView::setSelectionToolbarEnabled(bool enabled)
{
    m_selectionToolbarEnabled = enabled;
    if (!enabled) {
        page()->runJavaScript(
            QStringLiteral("var t=document.getElementById('breeze-sel-toolbar'); if(t) t.remove();"));
    } else {
        installSelectionToolbar();
    }
}

void WebView::installSelectionToolbar()
{
    if (!m_selectionToolbarEnabled)
        return;
    const QString js = QStringLiteral(R"JS(
(function(){
  if (window.__breezeSelToolbar) return;
  window.__breezeSelToolbar = true;
  var toolbar = null;
  function removeToolbar(){
    if (toolbar) { toolbar.remove(); toolbar = null; }
  }
  function mkBtn(txt, action, text){
    var b = document.createElement('button');
    b.textContent = txt;
    b.style.cssText = 'padding:4px 10px;margin:0 2px;border:1px solid #ccc;border-radius:4px;background:#fff;color:#333;cursor:pointer;font-size:12px;';
    b.onmousedown = function(ev){ ev.preventDefault(); ev.stopPropagation(); };
    b.onclick = function(ev){
      ev.preventDefault(); ev.stopPropagation();
      console.log('__BREEZE_SELECT__:' + action + ':' + text);
      removeToolbar();
    };
    return b;
  }
  document.addEventListener('mouseup', function(e){
    setTimeout(function(){
      var sel = window.getSelection();
      var text = sel ? sel.toString().trim() : '';
      if (!text || text.length < 2 || text.length > 2000) { removeToolbar(); return; }
      removeToolbar();
      toolbar = document.createElement('div');
      toolbar.id = 'breeze-sel-toolbar';
      toolbar.style.cssText = 'position:absolute;z-index:2147483647;background:#f8f8f8;border:1px solid #ccc;border-radius:6px;padding:3px;box-shadow:0 2px 8px rgba(0,0,0,0.15);';
      var r = sel.getRangeAt(0).getBoundingClientRect();
      toolbar.style.left = (window.scrollX + r.left) + 'px';
      toolbar.style.top  = (window.scrollY + r.bottom + 6) + 'px';
      toolbar.appendChild(mkBtn('解释', 'explain', text));
      toolbar.appendChild(mkBtn('翻译', 'translate', text));
      toolbar.appendChild(mkBtn('搜索', 'search', text));
      document.body.appendChild(toolbar);
    }, 10);
  }, true);
  document.addEventListener('mousedown', function(e){
    if (toolbar && !toolbar.contains(e.target)) removeToolbar();
  }, true);
  document.addEventListener('scroll', removeToolbar, true);
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
            menu.addAction(QStringLiteral("强制刷新（忽略缓存）"), this, [this]() {
                page()->triggerAction(QWebEnginePage::ReloadAndBypassCache);
            });
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
            menu.addAction(QStringLiteral("复制标题和地址"), this, [this]() {
                const QString t = title();
                QApplication::clipboard()->setText(
                    t.isEmpty() ? url().toString()
                                : t + QStringLiteral("\n") + url().toString());
            });
            menu.addAction(QStringLiteral("查看源代码"), this, [this]() {
                setUrl(QUrl(QStringLiteral("view-source:") + url().toString()));
            });

            connect(back, &QAction::triggered, this, &QWebEngineView::back);
            connect(forward, &QAction::triggered, this, &QWebEngineView::forward);

            menu.exec(event->globalPos());
        });
}

