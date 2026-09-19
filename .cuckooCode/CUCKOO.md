# CUCKOO.md

> Breeze —— 基于 Qt6 + Qt WebEngine 的 C++ 多标签浏览器

本文件面向 AI 编程助手（Cuckoo Code），说明本项目的结构、约定与开发流程，供在本仓库内进行代码修改时参考。

---

## 项目概览

- **项目名**：Breeze
- **当前版本**：2.4.5（定义于 `CMakeLists.txt` 的 `project(Breeze VERSION ...)`）
- **语言标准**：C++17
- **构建系统**：CMake 3.19+ + Ninja
- **GUI 框架**：Qt6 Widgets
- **渲染内核**：Qt WebEngine（Chromium）
- **平台**：Windows 10/11 x64
- **仓库**：https://github.com/HUUUU523/Breeze

---

## 环境要求

本机已验证的开发环境：

| 项目 | 路径 / 版本 |
|------|-------------|
| Qt | `D:\Qt\6.11.2\msvc2022_64` |
| 编译器 | MSVC 14.44.35207（VS Build Tools 2022，`D:\VSBuildTools`） |
| CMake | `D:\Tools\WinLibs\mingw64\bin\cmake.exe` |
| Ninja | `D:\Qt\Tools\Ninja` |
| 操作系统 | Windows x64 |

> ⚠️ **重要**：Qt WebEngine 只安装在 `msvc2022_64` 套件中，`mingw_64` 套件**没有** WebEngine。
> 因此必须使用 **MSVC 工具链**编译，不能用 MinGW。
>
> ⚠️ 构建（`cmake --build`）也必须在 **vcvars64 环境**下执行，否则会报
> `无法打开包括文件: "type_traits"`（缺少 MSVC 的 INCLUDE 变量）。

### 构建命令（务必在 vcvars64 下执行）

```bat
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:\Qt\6.11.2\msvc2022_64
cmake --build build
```

一键（推荐在 AI 环境中使用，整条命令自包含）：

```bat
cmd /c "call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build D:\C++\Breeze\build"
```

或使用附带的 `configure.bat`（加载环境并配置）：

```bat
configure.bat
cmake --build build
```

### 部署与运行

```bat
D:\Qt\6.11.2\msvc2022_64\bin\windeployqt.exe --release --no-translations build\Breeze.exe
set PATH=D:\Qt\6.11.2\msvc2022_64\bin;%PATH%
build\Breeze.exe
```

---

## 目录结构

```
Breeze/
├── CMakeLists.txt        # 构建配置（版本号在此定义，并注入 BREEZE_VERSION 宏）
├── configure.bat         # 一键配置脚本
├── app.rc                # Windows 资源（exe 图标，引用 B.ico）
├── resources.qrc         # Qt 资源（窗口图标 :/B.ico）
├── installer.iss         # Inno Setup 安装脚本
├── README.md             # 面向用户的说明
├── .github/workflows/build.yml   # CI：构建 + 签名 + 打包 + 发布
│
├── main.cpp              # 程序入口
├── browserwindow.h/.cpp  # 主窗口（标签、导航、地址栏、菜单、书签栏、会话）
├── webview.h/.cpp        # 自定义 QWebEngineView / QWebEnginePage
├── adblocker.h/.cpp      # 广告拦截 + 盾牌计数 + HTTPS 升级
├── downloadmanager.h/.cpp
├── historymanager.h/.cpp
├── settingsdialog.h/.cpp # 设置对话框 + 静态配置读写
├── bookmarkmanager.h/.cpp
├── bookmarksidebar.h/.cpp
├── syncmanager.h/.cpp    # WebDAV 云同步
├── syncdialog.h/.cpp
├── syncmerge.h/.cpp      # 同步数据合并（纯逻辑，可单测）
├── userscriptmanager.h/.cpp
├── translator.h/.cpp     # 多语言（代码内字典）
├── toolbox.h/.cpp        # 工具箱
├── qrcodegen.h/.cpp      # 二维码生成（内置，零依赖）
├── aimanager.h/.cpp      # AI 接口（OpenAI 兼容）
├── aidialog.h/.cpp
├── aisidebar.h/.cpp
├── cookiemanagerdialog.h/.cpp
├── updatemanager.h/.cpp  # GitHub Release 更新检查
├── logger.h/.cpp         # 文件日志
├── extension.h/.cpp      # 扩展系统（manifest 解析）
├── extensiondialog.h/.cpp
├── docs/ROADMAP.md       # 功能规划
└── tests/                # 单元测试（Qt Test + CTest）
```

---

## 数据存储

所有用户数据写入 **`QStandardPaths::AppDataLocation`**（即 `%APPDATA%\Breeze\Breeze\`），
**不写程序目录**（避免 Program Files 下无写权限导致静默失败）：

| 文件 | 内容 |
|------|------|
| `bookmarks.json` | 书签（含分组） |
| `history.json` | 历史记录 |
| `downloads.json` | 下载记录元数据 |
| `userscripts.json` | 用户脚本 / 用户样式 |
| `extensions.json` | 已加载扩展目录列表 |
| `logs/breeze.log` | 运行日志 |
| `profile/` | WebEngine 持久化存储 |
| `profile/cache/` | WebEngine 缓存 |

会话恢复数据存于 `QSettings`（组织名/应用名均为 `Breeze`）。
下载文件默认保存到系统「下载」目录（可在设置中指定默认下载目录）。

---

## 关键设计

### main.cpp
- 构造 `QApplication`，设置应用名/版本/组织名
- 配置默认 `QWebEngineProfile` 的持久化存储与缓存路径（AppData 下）
- 安装文件日志 `Logger::install()`
- 应用代理 `SettingsDialog::applyProxy()`
- 创建并显示 `BrowserWindow`

### BrowserWindow（browserwindow.h/.cpp）
主窗口，继承 `QMainWindow`，是本项目体量最大的文件。

关键成员：
- `QTabWidget *m_tabs`、`QLineEdit *m_urlBar`、`QProgressBar *m_progress`、`QToolBar *m_bookmarkBar`
- `QList<Bookmark> m_bookmarks`、`QList<HistoryEntry> m_history`、`QList<QUrl> m_closedTabs`
- `QSet<WebView*> m_pinnedTabs`、`DownloadManager *m_downloadManager` 等

关键方法：
- `createTabView()` —— 建 WebView、连接信号、加入标签栏，并注入「新窗口→新标签」回调
- `createTab(url, switchToTab)` —— 基于 createTabView 再加载 URL
- `restoreSession()` —— 按启动行为（0=恢复会话 / 1=主页 / 2=新标签页）初始化标签
- `saveSession()` / `saveBookmarks()` / `saveHistory()` —— 持久化
- `currentView()` —— 获取当前标签的 `WebView`
- `recordHistory(WebView*)` —— 按**实际加载完成**的标签记录历史（非当前标签）
- `onUrlEntered()` / `normalizedUrl()` —— 地址栏回车；网址直跳，否则按搜索引擎搜索
- `onLoadStarted/Progress/Finished` —— 处理加载状态（均判断 `sender() == currentView()`）

### WebView（webview.h/.cpp）
自定义 `QWebEngineView`：
- `setNewTabProvider(std::function<WebView*()>)` —— 注入「提供新标签视图」的回调
- 重写 `createWindow()` —— 新窗口请求时返回已加入标签栏的空 `WebView`，实现「真正在新标签打开」
- `BreezeWebPage` —— 重写 `javaScriptConsoleMessage`，解析状态栏悬停链接、划词工具等注入消息
- 右键菜单：复制链接/图片地址、在新标签打开、下载图片、AI 处理选中文字等

### SettingsDialog（settingsdialog.h/.cpp）
配置读写全部为**静态方法**（`QSettings`，组织名/应用名均为 `Breeze`）。
已支持的配置项：主页、默认搜索引擎、记录历史、启动时行为、新标签页行为、启动时检查更新、
下载限速、默认下载目录、下载完成后自动打开文件、网页最小字号、网页默认字号、代理、主题等。
新增配置项时，建议在头文件加 `static getter/setter` + 成员控件，在 `.cpp` 中：
构造里加控件 → `load()` 里读取 → `onAccepted()` 里保存。

---

## 开发约定

- 源文件统一 **UTF-8** 编码；CMake 对 MSVC 添加了 `/utf-8`
- 界面使用**纯代码构建**，不依赖 `.ui` 文件
- 使用 `CMAKE_AUTOMOC` 自动处理 Qt moc
- 字符串字面量优先使用 `QStringLiteral` 包装
- **用户数据一律写入 AppData**，不写程序目录
- 导航按钮使用 `QStyle` 标准图标，不依赖外部图片资源
- 修改现有代码时**尽量缩小改动范围**，不做无关格式化/重构

### 版本号维护
发版时需同步更新三处（保持一致的补丁号/次版本号）：
1. `CMakeLists.txt`：`project(Breeze VERSION x.y.z ...)`
2. `installer.iss`：`#define MyAppVersion "x.y.z"`
3. `README.md`：badge `version-x.y.z-blue`

改版本号后建议**重新配置**（`cmake -S . -B build ...`）再构建，使 `BREEZE_VERSION` 宏更新。

---

## 测试

`tests/test_sync.cpp` 为纯逻辑单元测试（同步数据合并 `SyncMerge::merge`），
使用 Qt Test + CTest。由 `BREEZE_BUILD_TESTS`（默认 ON）控制是否构建。

```bat
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## CI / 发布流程

`.github/workflows/build.yml`：
- 触发：push 到 `main`、打 `v*` tag、PR、手动
- 步骤：安装 Qt 6.8.1（qtwebengine/qtwebchannel/qtpositioning）→ MSVC 环境 →
  CMake 配置 → 构建 → `windeployqt` → 解码签名证书 → 签名 `Breeze.exe` →
  打包 zip → Inno Setup 生成安装包 → 签名安装包 → 上传 artifacts →
  若为 tag 则发布到 GitHub Release
- **代码签名**：依赖仓库 secrets `CODE_SIGN_PFX_BASE64`、`CODE_SIGN_PFX_PASSWORD`；
  自签名证书，签名后 Windows 仍可能显示「未知发布者」

发布新版本：更新版本号 → 构建验证 → 提交推送 → `git tag vX.Y.Z && git push origin vX.Y.Z`。

---

## 注意事项 / 已知坑

1. `readLines` / `read` 工具有行数上限（默认 2000），`browserwindow.cpp` 很大，需分段读取。
2. 用 `edit` 写含反斜杠转义（如 C++ 字面量里的 `\n`）的代码时，模板字符串可能把转义变成真实换行，
   破坏 C++ 字符串。**建议用 `QLatin1Char(10)` / `QChar(10)` / `join` 等替代 `"\n"`**。
3. Qt WebEngine API 差异：
   - `iconForUrl` 已移除 → 用 `QWebEngineProfile::requestIconForPageURL`
   - `certificateError` 是信号（非虚函数）
   - `QWebEngineFindTextResult` 为 `findText` 回调参数
   - `QUrl::queryItems()` 属于 `QUrlQuery`（需 `#include <QUrlQuery>`）
4. 构建工具函数有超时限制；大文件编译慢，建议后台构建 + 轮询日志文件。
5. 并发构建会因文件锁（`.d` 依赖文件、`.obj`）失败；启动新构建前确保没有残留的
   `cmake`/`ninja` 进程。
6. `.gitignore` 已忽略 `build/`、`*.log`、`*.pfx` 等，勿把构建产物或证书提交入库。

---

## 已修复的重要问题（历史记录，供维护参考）

1. **历史记错 URL** —— 后台标签加载完成时曾用 `currentView()` 记录，改为按 `sender()` 定位。
2. **进度条/导航按钮被后台标签污染** —— 已加 `sender() != currentView()` 判断。
3. **数据写在程序目录** —— 已改用 `AppDataLocation`。
4. **下载目录不可写** —— 已改用系统「下载」目录（并支持自定义）。
5. **书签右键菜单误配** —— 已改用 `widgetForAction()` 精确取按钮。
6. **下载映射悬空键** —— 已加 `destroyed` 连接清理。
7. **新窗口请求未处理** —— 已重写 `createWindow()` + provider 回调。
8. **工具栏用 Unicode 字符** —— 已换成 `QStyle` 标准图标。

---

## 后续可扩展方向（另见 docs/ROADMAP.md）

- 书签栏书签拖拽排序
- 下载多线程分片
- 完整扩展 API（chrome.storage / GM_xmlhttpRequest 等）
- 更完整的用户脚本元数据与 `@require` 支持
- 更多语言支持
