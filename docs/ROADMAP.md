# Breeze 功能扩展方案（ROADMAP）

> 版本基线：Breeze 2.0.0
> 文档目的：为 26 项候选功能提供可落地的设计方案，供后续分批实施。
> 现状核实时间：基于当前代码库扫描结果（非 CUCKOO.md 旧文档）。

---

## 0. 现状核实结论

对 26 项候选功能逐一扫描代码后，实际情况如下：

### 已实现（无需重复开发）

| 项 | 说明 | 证据 |
|----|------|------|
| 阅读模式 | 注入 overlay 提取正文 | `browserwindow.cpp` `toggleReaderMode()` |
| 标签拖动排序 | `m_tabs->setMovable(true)` | `browserwindow.cpp` L118 |
| 下载断点续传 | Range 头 + `resume()` | `downloadmanager.cpp` L104/L308 |
| 标签栏位置 | 顶部/左/右 | `setTabPosition()` |
| 鼠标手势 | 右键拖拽 后/前/刷新 | `setMouseGesturesEnabled()` |

### 未实现（本文档覆盖范围）

第 1~26 项中，除上述 5 项外均未实现。以下方案按模块分组。

---

## 一、体验类

### 1. 标签页增强（Pin / 静音 / 恢复关闭 / 溢出）

**目标**：标签固定、单标签静音、Ctrl+Shift+T 恢复刚关闭的标签、标签过多时可滚动。

**方案**：
- **Pin**：`QTabWidget` 原生不支持 pin。用 `QTabBar::setTabButton` + 自绘，或记录 `m_pinnedTabs: QSet<int>`，关闭时跳过 pinned、排序时把 pinned 固定在前。推荐后者（改动小）。
- **静音**：`QWebEnginePage::setAudioMuted(bool)`，在 `QTabBar` 右侧加自定义按钮（`setTabButton(index, QTabBar::RightSide, btn)`）。
- **恢复关闭**：维护 `QList<QUrl> m_closedTabs`（上限 20），`onCloseTab` 时压栈，Ctrl+Shift+T 弹栈新建。
- **溢出**：`QTabBar::setUsesScrollButtons(true)` + `setElideMode(Qt::ElideRight)`，一行代码。

**涉及文件**：`browserwindow.h/.cpp`
**工作量**：中（1~2 天）
**风险**：pin 的自绘按钮在样式切换时需同步更新。

---

### 2. 地址栏智能补全

**目标**：输入时下拉显示历史 + 书签匹配，选中即跳转。

**方案**：
- 给 `m_urlBar` 挂 `QCompleter`，model 用 `QStringListModel`。
- 数据源：`m_history`（title + url）+ `m_bookmarks`。
- 匹配模式：`Qt::MatchContains`，大小写不敏感。
- `QCompleter::activated` → `onUrlEntered()`。
- 数据变更（记录历史 / 增删书签）时刷新 model。

**涉及文件**：`browserwindow.h/.cpp`
**工作量**：小（半天）
**风险**：历史条目多时补全卡顿——限制 model 上限 500 条。

---

### 3. 中键 / Ctrl+点击后台打开

**目标**：中键点链接后台开标签；中键点标签关闭；Ctrl+点击链接后台开。

**方案**：
- **中键点链接**：Qt WebEngine 默认对中键点击会走 `createWindow(type == WebBrowserBackgroundTab)`。当前 `createWindow` 忽略 type，改为：
  - `WebBrowserTab` → 前台新标签
  - `WebBrowserBackgroundTab` → 后台新标签（`createTab(url, false)`）
  - `WebBrowserWindow` → 新窗口（当前行为）
- **中键点标签关闭**：给 `m_tabs->tabBar()` 装 eventFilter，`QEvent::MouseButtonRelease` + `Qt::MiddleButton` → `onCloseTab(index)`。

**涉及文件**：`webview.cpp`（createWindow 分流）、`browserwindow.cpp`（tabBar eventFilter）
**工作量**：小（半天）
**风险**：极低。

---

### 4. 状态栏悬停显示链接地址

**目标**：鼠标悬停链接时，状态栏显示目标 URL。

**方案**：
- `QWebEnginePage` 没有直接 linkHovered 信号（QtWebEngine 移除了）。
- 用注入 JS 监听 `mouseover` / `mouseout`，把 `a.href` 通过 `QWebChannel` 回传，或简单点：用 `runJavaScript` 在 `loadFinished` 时注入监听，通过 `document.title` 临时传值（hack，不推荐）。
- **推荐**：`QWebChannel` 建一个 `Bridge` QObject，暴露 `setHoveredUrl(QString)`，页面注入 JS 调 `bridge.setHoveredUrl(...)`，C++ 侧更新 `statusBar()->showMessage()`。

**涉及文件**：新增 `webchannelbridge.h/.cpp`；`browserwindow.cpp` 注入脚本
**工作量**：中（1 天）
**风险**：QWebChannel 需在 profile 上注册，注意与现有 profile 共存。

---

### 5. 记忆页面缩放（按域名）

**目标**：每个站点记住自己的缩放比例，切换标签/重启后保留。

**方案**：
- 存储：`QSettings("Breeze","Breeze")` 下 `zoom/<host>` → double。
- `QWebEngineView::setZoomFactor` 在 `loadFinished` 时按 host 应用。
- 缩放操作时写入当前 host。
- 现有 `applyZoom(double delta)` 改造：改为绝对 `setZoomFactor`，并持久化。

**涉及文件**：`browserwindow.h/.cpp`
**工作量**：小（半天）
**风险**：隐私标签不持久化（按现有约定）。

---

## 二、功能性

### 6. 书签栏图标化 + 拖拽排序

**目标**：书签栏用 favicon 图标 + 文字；拖动可排序。

**方案**：
- **图标**：`QWebEngineProfile::iconForUrl()` 或异步 `QWebEnginePage::iconUrl` 获取 favicon，缓存到内存 `QHash<QString,QIcon>`。
- **拖拽**：当前书签栏是 `QToolBar + QAction`，不支持拖动。改为自定义 `QWidget`（`QListWidget` 横向 + `InternalMove`）或给 toolbar 装 drag-drop eventFilter。
- **推荐**：把书签栏换成 `QListView`（IconMode，横向），model 为 `m_bookmarks`，天然支持拖动排序。

**涉及文件**：`browserwindow.h/.cpp`（`rebuildBookmarkBar` / `addBookmarkAction` 重写）
**工作量**：大（2~3 天）
**风险**：书签栏从 toolbar 换 listview，涉及右键菜单、分组下拉的迁移，回归面大。

---

### 7. 下载增强（多线程分片 / 通知 / 限速）

**目标**：大文件多线程下载、完成时系统通知、可选限速。

**方案**：
- **多线程分片**：当前 `DownloadManager` 用 `QNetworkAccessManager` 单请求 + Range 续传。改多线程需自研分片调度：`HEAD` 拿 `Content-Length` 和 `Accept-Ranges`，切 N 段，每段独立 `QNetworkReply`，写同一文件的不同 offset（`QFile::seek`）。合并需线程安全。
- **通知**：`QSystemTrayIcon::showMessage()` 或 `QNotification`（平台相关），Windows 下用 `QSystemTrayIcon` 即可。
- **限速**：`QNetworkReply::setReadBufferSize()` 效果有限，精确限速需在读 readyRead 后 `QThread::msleep` 节流。

**涉及文件**：`downloadmanager.h/.cpp`（大改）
**工作量**：大（3~5 天）
**风险**：多线程写文件、断点续传与分片叠加、进度合并计算，复杂度高。建议先做通知（小），分片单独一期。

---

### 8. 阅读模式增强

**目标**：现有阅读模式支持字号调节、背景色切换、行距。

**方案**：
- 现状：overlay 固定字号 19px、背景 #f5f2ea。
- 增强：在 overlay 工具栏加 A-/A+/背景按钮，改 `inner.style.fontSize` / `overlay.style.background`。
- 纯前端 JS 改动，不涉及 C++。

**涉及文件**：`browserwindow.cpp` `toggleReaderMode()` 内 JS 字符串
**工作量**：小（半天）
**风险**：无。

---

### 9. 页面截图

**目标**：可视区域截图 / 整页截图，保存 PNG。

**方案**：
- 可视区域：`QWidget::grab()` → `QPixmap::save()`。
- 整页：`QWebEnginePage::printToPdf()` 拿 PDF 后转图不便；替代用注入 JS 计算 `document.body.scrollHeight`，滚动 + 分段 grab 拼接（hack）。或用 `QWebEngineView::renderProcessPid` 配合原生截图（复杂）。
- **推荐**：先做可视区域（简单），整页用「分段滚动 + 拼接」。

**涉及文件**：`browserwindow.h/.cpp` 新增 slot
**工作量**：小~中（1 天）
**风险**：分段拼接在固定定位元素上会错位。

---

### 10. 代理设置

**目标**：支持 HTTP/SOCKS5 代理，全局或按规则。

**方案**：
- `QNetworkProxy::setApplicationProxy()` 全局生效（影响所有网络请求，含 WebEngine）。
- 配置项存 `QSettings`：`proxy/type`、`proxy/host`、`proxy/port`、`proxy/user`、`proxy/pass`。
- 设置界面加到 `SettingsDialog`。
- 按规则分流需自研 PAC，工作量大，先做全局。

**涉及文件**：`settingsdialog.h/.cpp`、`main.cpp`（启动时应用代理）
**工作量**：小（半天）
**风险**：WebEngine 对应用级代理的遵循需实测。

---

### 11. Cookie / 缓存管理界面

**目标**：查看、清除指定站点 Cookie 与缓存。

**方案**：
- `QWebEngineProfile::cookieStore()` → `QWebEngineCookieStore`，`deleteAllCookies()` / `deleteCookie()`。
- 浏览 Cookie：`cookieStore()->loadAllCookies()` 信号拿列表（Qt 6.5+）。
- 清缓存：`profile->clearHttpCache()`。
- 界面：新对话框 `DataManagerDialog`，列出 Cookie、按钮清除。

**涉及文件**：新增 `datamanagerdialog.h/.cpp`；`browserwindow.cpp` 菜单入口
**工作量**：中（1~2 天）
**风险**：`loadAllCookies` 在部分 Qt 版本行为差异，需实测。

---

### 12. 右键「用其他引擎搜索选中文字」

**目标**：右键选中文字后，可指定用某个搜索引擎搜。

**方案**：
- 现有 `WebView::contextMenuEvent` 已有选中文字处理，加子菜单「搜索」。
- 子菜单列：默认引擎 + 已配置的其他引擎（Bing/Google/Baidu/DuckDuckGo）。
- 触发 → `emit searchRequested(engine, text)` → `BrowserWindow` 建新标签跳转。

**涉及文件**：`webview.h/.cpp`、`browserwindow.cpp`
**工作量**：小（半天）
**风险**：无。

---

## 三、扩展机制类

### 13. UserScript 增强（@run-at / @grant / GM_* / @require）

**目标**：支持脚本元数据，控制注入时机与 API。

**方案**：
- **解析元数据**：现有 `UserScriptManager` 存脚本内容，加解析 `// ==UserScript==` 块，提取 `@run-at`、`@grant`、`@require`、`@match`。
- **注入时机**：
  - `document-start`：用 `QWebEngineScript` + `InjectionPoint::DocumentCreation`（现有注入是 loadFinished 后，偏晚）。
  - `document-end` / `document-idle`：`Deferred` / `loadFinished`。
  - 改为 `QWebEngineScriptCollection` 注册，比 `runJavaScript` 更规范。
- **GM_ API**：实现 `GM_setValue` / `GM_getValue`（存 `QSettings` 或本地 JSON）、`GM_xmlhttpRequest`（用 `QNetworkAccessManager` + `QWebChannel` 桥接）。
- **@require**：加载时把外部 JS 内容下载并拼接。

**涉及文件**：`userscriptmanager.h/.cpp`（大改）、`browserwindow.cpp` `injectUserScripts`
**工作量**：大（3~5 天）
**风险**：QWebEngineScript 的注入时机与 profile 绑定，需重构注入通道；GM_xmlhttpRequest 的跨域需桥接。

---

### 14. 脚本在线安装

**目标**：点击 `.user.js` 链接自动弹出安装确认。

**方案**：
- 拦截下载：`QWebEngineProfile::downloadRequested`，若 `suggestedFileName()` 以 `.user.js` 结尾，读内容 → 弹 `UserScriptManager` 安装确认框。
- 或在 `WebView::createWindow` / `acceptNavigationRequest` 里判断 URL 后缀。
- 确认后写入 `userscripts.json`。

**涉及文件**：`browserwindow.cpp`（downloadRequested 分支）、`userscriptmanager.h/.cpp`
**工作量**：中（1~2 天）
**风险**：依赖 13 的元数据解析（安装时要显示 @name/@match）。

---

## 四、安全与隐私

### 15. HTTPS 强制升级 + 证书错误提示

**目标**：HTTP 自动升级 HTTPS；证书错误显示友好页而非直接阻断。

**方案**：
- **升级**：`QWebEngineUrlRequestInterceptor`（现有 `AdBlocker` 已是此接口）里，对 `http://` 请求改 `https://`，失败回退。需在 `AdBlocker::interceptRequest` 加分支（或新建 interceptor 链）。
- **证书错误**：`QWebEnginePage::certificateError()` 默认拒绝。可重写该虚函数显示「继续访问」按钮。

**涉及文件**：`adblocker.h/.cpp`（或新增 interceptor）、`webview.h/.cpp`（certificateError）
**工作量**：中（1~2 天）
**风险**：HTTP→HTTPS 升级对不支持 HTTPS 的站点会导致访问失败，需回退逻辑。

---

### 16. 跟踪器拦截可视化（盾牌）

**目标**：地址栏旁盾牌图标，显示本页拦截数量，点击看详情。

**方案**：
- `AdBlocker` 拦截时计数（按 page），emit `blockedCountChanged(int)`。
- `BrowserWindow` 在地址栏右侧加 `QLabel`（盾牌图标），显示计数。
- 点击弹出已拦截域名列表（需 AdBlocker 记录最近拦截的 URL）。

**涉及文件**：`adblocker.h/.cpp`（计数 + 记录）、`browserwindow.h/.cpp`（UI）
**工作量**：中（1~2 天）
**风险**：需区分「当前页」计数，AdBlocker 是全局 interceptor，要按 page 关联。

---

### 17. DNS over HTTPS

**目标**：支持 DoH 解析。

**方案**：
- Qt WebEngine 支持通过命令行参数 `--dns-over-https-templates=` 或 `QWebEngineProfile` 设置（视 Qt 版本）。
- Qt 6.5+ 可用 `QWebEngineProfile::setDnsOverHttpsMode` 之类（需查 API）。
- 备选：启动参数注入。

**涉及文件**：`main.cpp`（启动参数）、`settingsdialog`（配置）
**工作量**：小（半天），但依赖 Qt 版本能力
**风险**：Qt 6.11 的 API 支持情况需实测确认。

---

### 18. 隐私模式增强（独立窗口 + 一键清除）

**目标**：隐私模式用独立窗口而非标签；提供一键清除浏览数据。

**方案**：
- **独立窗口**：新建 `BrowserWindow` 实例，传入 `privateProfile`，不共享 `m_tabs`。需 `BrowserWindow` 构造函数支持 profile 参数。
- **一键清除**：菜单加「清除浏览数据」，弹对话框选（历史/缓存/Cookie/下载记录），调对应 clear API。

**涉及文件**：`browserwindow.h/.cpp`（构造重载）、`main.cpp`
**工作量**：中（1~2 天）
**风险**：现有隐私标签逻辑要迁移，回归面。

---

## 五、AI 方向

### 19. 划词工具栏

**目标**：选中文字后浮出小按钮（翻译/解释/搜索）。

**方案**：
- 注入 JS 监听 `mouseup`，若有选区则在光标处画浮动 div（按钮）。
- 点击 → `QWebChannel` 回传 action + text → C++ 调现有 `aiActionRequested` 逻辑。
- 复用 `QWebChannel`（与 #4 共用 Bridge）。

**涉及文件**：`browserwindow.cpp`（注入脚本）、Bridge
**工作量**：中（1 天）
**风险**：与页面自身选区 UI 冲突，需可开关。

---

### 20. AI 侧边栏

**目标**：常驻侧边对话面板，可带入当前页上下文。

**方案**：
- `QDockWidget` 停靠右侧，内嵌对话 UI（复用 `AiDialog` 的组件逻辑）。
- `BrowserWindow` 提供 `currentPageText()` 给侧边栏作为 context。

**涉及文件**：新增 `aisidebar.h/.cpp`（或重构 `aidialog`）；`browserwindow.cpp`
**工作量**：中（2 天）
**风险**：`AiDialog` 是对话框，改成 dock 需抽取对话组件。

---

### 21. 网页问答（RAG）

**目标**：把当前页当知识库，问答。

**方案**：
- 提取正文（复用阅读模式的提取 JS）→ 分块 → 存内存。
- 提问时把相关块 + 问题拼 prompt 发给 `AiManager`。
- 简单版：整页正文直接塞 context（受 token 限制）；进阶版：本地向量检索（工作量大）。

**涉及文件**：`aimanager.h/.cpp`、新增 QA 界面
**工作量**：中~大（2~4 天）
**风险**：长文超 token；检索需额外依赖。

---

### 22. 本地模型（Ollama）优化

**目标**：自动拉取 Ollama 模型列表供选择。

**方案**：
- `AiManager` 加 `GET /api/tags` 请求，解析模型名列表。
- `AiDialog` 的模型输入改为可编辑下拉，填充列表。

**涉及文件**：`aimanager.h/.cpp`、`aidialog.h/.cpp`
**工作量**：小（半天）
**风险**：无。

---

## 六、工程 / 维护类

### 23. 自动更新

**目标**：检查 GitHub Release，提示下载。

**方案**：
- `QNetworkAccessManager` GET `https://api.github.com/repos/<owner>/<repo>/releases/latest`。
- 比较 `tag_name` 与 `BREEZE_VERSION` 宏。
- 有新版 → 提示 + 打开下载页（`QDesktopServices::openUrl`）。
- 可选：下载 `Setup.exe` 并启动。

**涉及文件**：新增 `updatemanager.h/.cpp`；`browserwindow.cpp` 菜单入口
**工作量**：小（半天）
**风险**：仓库地址需配置；GitHub API 限流。

---

### 24. 崩溃恢复与日志

**目标**：日志落盘，异常可追溯。

**方案**：
- `qInstallMessageHandler` 自定义 handler，写 `AppData/logs/breeze.log`（按天滚动）。
- 捕获 `qFatal` / 未处理异常（Windows 下 `SetUnhandledExceptionFilter`）。
- 启动时若检测到上次异常退出（写标志位），提示恢复会话。

**涉及文件**：新增 `logger.h/.cpp`；`main.cpp`
**工作量**：小（半天）
**风险**：无。

---

### 25. 单元测试

**目标**：对纯逻辑（书签合并、URL 规范化、同步加密）加测试。

**方案**：
- 用 `Qt Test`（`QTest`）+ CTest。
- 抽离纯函数为独立模块（现在这些逻辑在 `BrowserWindow` 里，耦合 UI）。
- 测试项：`normalizedUrl`、`mergeSyncData`、加密解密往返。

**涉及文件**：新增 `tests/` 目录；`CMakeLists.txt` 加 `enable_testing()`
**工作量**：中（1~2 天），需先重构抽离
**风险**：重构可能引入回归。

---

### 26. 扩展系统（Chrome 扩展 API 子集）

**目标**：支持加载 Chrome 扩展（MV3 子集）。

**方案**：
- 这是最大工程，需实现 `chrome.*` API 的 JS shim + 权限模型 + manifest 解析 + content script 注入 + background service worker（WebEngine 无 SW，需模拟）。
- 建议单独立项，不在本 ROADMAP 短期范围。

**涉及文件**：全新模块
**工作量**：极大（数周~数月）
**风险**：极高，建议评估必要性后再决定。

---

## 建议实施批次

按「性价比 + 依赖关系」排序：

**第 1 批（低风险、半天~1 天，快速见效）**
- #4 状态栏悬停链接
- #5 按域名记忆缩放
- #3 中键/Ctrl+点击后台打开
- #12 右键其他引擎搜索
- #8 阅读模式增强
- #22 Ollama 模型列表
- #23 自动更新
- #24 日志落盘

**第 2 批（中等，1~2 天）**
- #2 地址栏补全
- #1 标签页增强
- #9 页面截图
- #10 代理设置
- #16 盾牌计数
- #19 划词工具栏
- #15 HTTPS 升级 + 证书页
- #18 隐私模式增强

**第 3 批（较大，2~5 天）**
- #6 书签栏图标化 + 拖拽
- #11 Cookie/缓存管理
- #13 UserScript 增强
- #14 脚本在线安装
- #20 AI 侧边栏
- #21 网页问答
- #7 下载多线程（可拆出通知先做）

**第 4 批（长期）**
- #25 单元测试
- #26 扩展系统

---

## 公共依赖说明

- **QWebChannel Bridge**：#4、#19 都依赖「页面→C++」通信通道，建议第 1 批一并搭好，后续复用。
- **AppData 存储约定**：新增持久化（缩放、代理、日志、脚本设置）统一走 `QStandardPaths::AppDataLocation`，与现有约定一致。
- **QSettings 命名空间**：统一 `QSettings("Breeze","Breeze")`。

---

## 待确认事项

1. GitHub 仓库地址（自动更新用）——需提供 owner/repo。
2. Qt 6.11 是否支持 DoH API ——需实测后再定 #17 方案。
3. 是否需要保留 `CUCKOO.md` 旧文档，或同步更新到 2.0.0 现状。
