#include "webview.h"
#include "adblocker.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QContextMenuEvent>
#include <QMenu>
#include <QWebEngineCertificateError>
#include <QWebEngineContextMenuRequest>
#include <QWebEngineHistory>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QMessageBox>

// ===================== BreezeWebPage =====================

namespace {
const char *kHoverPrefix  = "__BREEZE_HOVER__:";
const char *kSelectPrefix = "__BREEZE_SELECT__:";
const char *kExtMsgPrefix = "__BREEZE_EXTMSG__:";
const char *kTabsCmdPrefix = "__BREEZE_TABSCMD__:";
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
    if (message.startsWith(QLatin1String(kExtMsgPrefix))) {
        emit extMessage(message.mid(int(qstrlen(kExtMsgPrefix))));
        return;
    }
    if (message.startsWith(QLatin1String(kTabsCmdPrefix))) {
        emit tabsCommand(message.mid(int(qstrlen(kTabsCmdPrefix))));
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
    connect(bp, &BreezeWebPage::extMessage,
            this, &WebView::extMessage);
    connect(bp, &BreezeWebPage::tabsCommand,
            this, &WebView::tabsCommand);
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

// ===================== 鼠标手势 =====================

void WebView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        m_gestureActive = true;
        m_gestureStart = event->pos();
        m_gestureLast = event->pos();
        m_gestureDirs.clear();
        event->accept();
        return;
    }
    QWebEngineView::mousePressEvent(event);
}

void WebView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_gestureActive) {
        const QPoint delta = event->pos() - m_gestureLast;
        constexpr int kThreshold = 30;   // 移动多少像素算一个方向
        if (delta.manhattanLength() >= kThreshold) {
            QString dir;
            if (qAbs(delta.x()) > qAbs(delta.y()))
                dir = delta.x() > 0 ? QStringLiteral("R") : QStringLiteral("L");
            else
                dir = delta.y() > 0 ? QStringLiteral("D") : QStringLiteral("U");
            if (m_gestureDirs.right(1) != dir)
                m_gestureDirs += dir;
            m_gestureLast = event->pos();
        }
        emit gestureProgress(event->pos());
        event->accept();
        return;
    }
    QWebEngineView::mouseMoveEvent(event);
}

void WebView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && m_gestureActive) {
        m_gestureActive = false;
        const int moved = (event->pos() - m_gestureStart).manhattanLength();
        if (moved < 20) {
            // 没怎么动：视为普通右键菜单
            QContextMenuEvent ce(QContextMenuEvent::Mouse, event->pos(),
                                 mapToGlobal(event->pos()));
            contextMenuEvent(&ce);
        } else if (!m_gestureDirs.isEmpty()) {
            emit gestureTriggered(m_gestureDirs);
        }
        event->accept();
        return;
    }
    QWebEngineView::mouseReleaseEvent(event);
}

void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    // 记录本次右键的上下文（图片/链接地址）
    const QUrl mediaUrl = lastContextMenuRequest()
        ? lastContextMenuRequest()->mediaUrl() : QUrl();
    const QUrl linkUrl = lastContextMenuRequest()
        ? lastContextMenuRequest()->linkUrl() : QUrl();

    // 先异步取选中文字，再弹菜单
    page()->runJavaScript(
        QStringLiteral("window.getSelection().toString()"),
        [this, event, mediaUrl, linkUrl](const QVariant &result) {
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

                // 复制为 Markdown
                menu.addAction(QStringLiteral("复制为 Markdown 链接"), this, [this, t]() {
                    const QString md = QStringLiteral("[%1](%2)")
                        .arg(t.left(80).replace(QLatin1Char(']'), QLatin1Char(')')),
                             url().toString());
                    QApplication::clipboard()->setText(md);
                });
                // 高亮选中文字
                menu.addAction(QStringLiteral("🖍 高亮选中"), this, [this]() {
                    page()->runJavaScript(QStringLiteral(
                        "(function(){"
                        "var s=window.getSelection();"
                        "if(!s.rangeCount)return;"
                        "var r=s.getRangeAt(0);"
                        "var span=document.createElement('mark');"
                        "span.style.cssText='background:#ffe066;padding:1px 2px;border-radius:2px;';"
                        "span.setAttribute('data-breeze-mark','1');"
                        "try{span.appendChild(r.extractContents());r.insertNode(span);}"
                        "catch(e){}"
                        "s.removeAllRanges();"
                        "})();"));
                });
                menu.addSeparator();
            }

            if (linkUrl.isValid() && !linkUrl.isEmpty()) {
                menu.addAction(QStringLiteral("复制链接地址"), this, [linkUrl]() {
                    QApplication::clipboard()->setText(linkUrl.toString());
                });
                menu.addAction(QStringLiteral("在新标签打开链接"), this, [this, linkUrl]() {
                    emit newTabRequested(linkUrl, true);
                });
            }
            if (mediaUrl.isValid() && !mediaUrl.isEmpty()) {
                menu.addAction(QStringLiteral("复制图片地址"), this, [mediaUrl]() {
                    QApplication::clipboard()->setText(mediaUrl.toString());
                });
                menu.addAction(QStringLiteral("在新标签打开图片"), this, [this, mediaUrl]() {
                    emit newTabRequested(mediaUrl, true);
                });
                menu.addAction(QStringLiteral("下载图片"), this, [mediaUrl]() {
                    QNetworkAccessManager *nam = new QNetworkAccessManager;
                    QNetworkRequest req(mediaUrl);
                    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Breeze"));
                    QNetworkReply *reply = nam->get(req);
                    QObject::connect(reply, &QNetworkReply::finished, nam,
                                     [reply, nam, mediaUrl]() {
                        reply->deleteLater();
                        nam->deleteLater();
                        if (reply->error() != QNetworkReply::NoError)
                            return;
                        QString name = QFileInfo(mediaUrl.path()).fileName();
                        if (name.isEmpty())
                            name = QStringLiteral("image.png");
                        const QString dir =
                            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
                        const QString path = QDir(dir).filePath(name);
                        QFile f(path);
                        if (f.open(QIODevice::WriteOnly))
                            f.write(reply->readAll());
                    });
                });
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
                emit newTabRequested(
                    QUrl(QStringLiteral("view-source:") + url().toString()), true);
            });
            menu.addAction(QStringLiteral("清除本页高亮"), this, [this]() {
                page()->runJavaScript(QStringLiteral(
                    "(function(){"
                    "document.querySelectorAll('mark[data-breeze-mark]').forEach(function(m){"
                    "var t=document.createTextNode(m.textContent);"
                    "m.parentNode.replaceChild(t,m);});"
                    "})();"));
            });
            menu.addAction(QStringLiteral("📺 画中画"), this, [this]() {
                page()->runJavaScript(QStringLiteral(
                    "(function(){"
                    "var vs=document.querySelectorAll('video');"
                    "var best=null,bestArea=0;"
                    "for(var i=0;i<vs.length;i++){"
                    "var v=vs[i];var a=(v.clientWidth||0)*(v.clientHeight||0);"
                    "if(a>bestArea){bestArea=a;best=v;}}"
                    "if(!best){alert('未找到视频');return;}"
                    "if(document.pictureInPictureElement){"
                    "document.exitPictureInPicture();}"
                    "else if(best.requestPictureInPicture){"
                    "best.requestPictureInPicture().catch(function(e){alert('画中画失败: '+e);});"
                    "}else{alert('浏览器不支持画中画');}"
                    "})();"));
            });
            menu.addSeparator();

            // 阅读模式：提取正文，清爽排版
            menu.addAction(QStringLiteral("📖 阅读模式"), this, [this]() {
                page()->runJavaScript(QStringLiteral(
                    "(function(){"
                    "if(document.getElementById('__breeze_reader__')){"
                    "document.getElementById('__breeze_reader__').remove();return;}"
                    "var cands=document.querySelectorAll('article,main,[class*=content],"
                    "[class*=post],[class*=article],[id*=content],[id*=article]');"
                    "var best='',bestLen=0;"
                    "for(var i=0;i<cands.length;i++){"
                    "var t=cands[i].innerText||'';"
                    "if(t.length>bestLen){bestLen=t.length;best=t;}}"
                    "if(!bestLen||bestLen<200)best=document.body.innerText;"
                    "var d=document.createElement('div');"
                    "d.id='__breeze_reader__';"
                    "d.style.cssText='position:fixed;inset:0;z-index:2147483646;"
                    "background:#faf8f5;color:#333;overflow:auto;padding:60px 20vw;"
                    "font:18px/1.8 Georgia,serif;white-space:pre-wrap;';"
                    "d.textContent=best;"
                    "var btn=document.createElement('button');"
                    "btn.textContent='× 退出阅读模式';"
                    "btn.style.cssText='position:fixed;top:20px;right:20px;z-index:2147483647;"
                    "padding:8px 16px;border:none;border-radius:6px;background:#3a6ea5;"
                    "color:#fff;font-size:14px;cursor:pointer;';"
                    "btn.onclick=function(){d.remove();btn.remove();};"
                    "document.body.appendChild(d);document.body.appendChild(btn);"
                    "})();"));
            });

            // 暗黑模式：反色滤镜
            menu.addAction(QStringLiteral("🌙 暗黑模式（反色）"), this, [this]() {
                page()->runJavaScript(QStringLiteral(
                    "(function(){"
                    "var id='__breeze_dark__';"
                    "var s=document.getElementById(id);"
                    "if(s){s.remove();return;}"
                    "var e=document.createElement('style');e.id=id;"
                    "e.textContent='html{filter:invert(1) hue-rotate(180deg)!important;}"
                    "img,video,iframe,canvas,svg{filter:invert(1) hue-rotate(180deg)!important;}';"
                    "document.head.appendChild(e);"
                    "})();"));
            });

            connect(back, &QAction::triggered, this, &QWebEngineView::back);
            connect(forward, &QAction::triggered, this, &QWebEngineView::forward);

            menu.exec(event->globalPos());
        });
}

