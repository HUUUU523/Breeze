# CUCKOO.md

> Breeze —— 基于 Qt6 + Qt WebEngine 的 C++ 多标签浏览器

## 项目概览

Breeze 是一个用 C++ / Qt6 编写的多标签浏览器，渲染内核采用 Qt WebEngine（Chromium）。

- **项目名**：Breeze
- **版本**：1.0.0
- **语言标准**：C++17
- **构建系统**：CMake + Ninja
- **GUI 框架**：Qt6 Widgets
- **渲染内核**：Qt WebEngine (Chromium)

## 环境要求

本机已验证的开发环境：

| 项目 | 路径 / 版本 |
|------|-------------|
| Qt | `D:\Qt\6.11.2\msvc2022_64` |
| 编译器 | MSVC 14.44.35207（VS Build Tools 2022，`D:\VSBuildTools`） |
| CMake | `D:\Tools\WinLibs\mingw64\bin\cmake.exe` |
| Ninja | `D:\Qt\Tools\Ninja` |
| 操作系统 | Windows x64 |

> ⚠️ **重要**：Qt WebEngine 只安装在 `msvc2022_64` 套件中，`mingw_64` 套件 **没有** WebEngine。
> 因此必须使用 **MSVC 工具链** 编译，不能用 MinGW。
>
> ⚠️ 构建（`cmake --build`）也必须在 **vcvars64 环境**下执行，否则会报
> `无法打开包括文件: "type_traits"`（缺少 MSVC 的 INCLUDE 变量）。

## 目录结构

```
Breeze/
├── CMakeLists.txt        # CMake 构建配置
├── configure.bat         # 一键配置脚本（加载 MSVC 环境 + 运行 CMake）
├── main.cpp              # 程序入口，QApplication / Profile 初始化
├── browserwindow.h       # 主窗口类声明
├── browserwindow.cpp     # 主窗口实现（标签、导航、地址栏、进度、书签、历史）
├── webview.h             # 自定义 QWebEngineView（新窗口请求处理）
├── webview.cpp           # 自定义 QWebEngineView 实现
├── downloadmanager.h     # 下载管理窗口
├── downloadmanager.cpp   # 下载管理窗口实现
├── historymanager.h      # 历史记录窗口
├── historymanager.cpp    # 历史记录窗口实现
├── settingsdialog.h      # 设置对话框（静态配置读写）
├── settingsdialog.cpp    # 设置对话框实现
├── bookmarkmanager.h     # 书签管理对话框（树形、拖拽分组）
├── bookmarkmanager.cpp   # 书签管理对话框实现
├── syncmanager.h         # WebDAV 云同步（加密上传/下载）
├── syncmanager.cpp       # 云同步实现
├── syncdialog.h          # 云同步配置对话框
├── syncdialog.cpp        # 云同步配置实现
├── userscriptmanager.h   # 用户脚本 / 用户样式管理
├── userscriptmanager.cpp # 用户脚本 / 用户样式实现
├── translator.h          # 多语言（代码内字典）
├── translator.cpp        # 多语言实现
├── toolbox.h             # 工具箱（7 个实用工具）
├── toolbox.cpp           # 工具箱实现
├── qrcodegen.h           # 二维码生成（内置，零依赖）
└── qrcodegen.cpp         # 二维码生成实现
```

## 功能特性

已实现：

- **多标签页**（`QTabWidget`，支持关闭、拖动排序、至少保留一个）
- **地址栏**（回车跳转；网址自动补 `https://`；非网址按搜索引擎搜索）
- **导航按钮**：后退 / 前进 / 刷新 / 停止 / 主页（使用 `QStyle` 标准图标）
- **加载进度条**（右下角状态栏，仅反映当前标签）
- **标签标题与 URL 自动同步**
- **新窗口请求 → 新标签**（`target=_blank`、`window.open` 真正在新标签打开）
- **书签栏 + 书签管理**（顶部工具栏，点击跳转，右键删除，JSON 持久化）
- **下载管理窗口**（`QTableWidget`，实时进度条，暂停/继续、取消、打开文件夹）
- **历史记录窗口**（搜索过滤、双击跳转、清空、JSON 持久化，最多 500 条）
- **设置对话框**（主页、默认搜索引擎、是否记录历史，存于 `QSettings`）
- **书签管理对话框**（树形显示分组/书签，拖拽移动分组，增删改）
- **深色主题**（跟随系统 / 浅色 / 深色，工具栏 🎨 菜单切换，系统主题变化自动响应）
- **云同步**（WebDAV，书签+历史打包加密上传/下载，口令派生密钥 + 随机 IV）
- **双向同步合并**（上传前先下载云端，按 URL 合并书签/历史，冲突取较新记录）
- **用户脚本（UserScript / UserStyle）**（JS 或 CSS，URL 通配匹配，页面加载后自动注入）
- **多语言**（中文 / English，工具栏 🌐 菜单切换，代码内字典）
- **AI 助手**（OpenAI 兼容接口，菜单「AI 对话…」「AI 总结当前页」）：
  - 支持 OpenAI / DeepSeek / 通义千问 / Kimi / Ollama 等所有 `/v1/chat/completions` 服务
  - 多轮对话窗口，接口地址/API Key/模型名可配置（存 QSettings）
  - 一键提取当前页正文并让 AI 总结
- **工具箱**（工具栏 🧰，7 个标签页）：
  - JSON 格式化 / 压缩
  - URL 编码解码、Base64 编码解码
  - 时间戳 ↔ 日期转换
  - 随机密码生成
  - 颜色 HEX ↔ RGB 转换 + 取色器
  - 单位换算（长度 / 重量 / 温度）
  - 二维码生成（内置 QR 编码器，保存为 PNG）
- **下载断点续传**（QNetworkAccessManager + Range 头，中断后从断点继续）
- **页面查找**（Ctrl+F，输入即高亮，上一个/下一个，Esc 关闭）
- **页面缩放**（Ctrl+= 放大、Ctrl+- 缩小、Ctrl+0 复位，范围 25%~500%）
- **打印 / 导出 PDF**（Ctrl+P 生成打印文件并打开；Ctrl+Shift+P 保存为 PDF）
- **会话恢复**（关闭时保存标签页，下次启动恢复，隐私标签不保存）
- **隐私模式标签页**（Ctrl+Shift+N，独立非持久化 Profile，不写盘、不记历史/会话）
- **书签分组**（书签可归属分组，书签栏按分组显示为下拉菜单）
- **历史记录按日期分组**（今天 / 昨天 / yyyy-MM-dd 分组标题）
- **下载记录持久化**（下载元数据存 `downloads.json`，重启后仍可见）
- **快捷键**：Ctrl+T 新标签、Ctrl+Shift+N 隐私标签、Ctrl+W 关标签、
  Ctrl+L 聚焦地址栏、Ctrl+F 查找、F5/Ctrl+R 刷新、Alt+←/→ 前进后退、
  Ctrl+Tab 切换标签、Ctrl+P 打印、Ctrl+Shift+P 存 PDF、Ctrl+=/-/0 缩放
- **持久化 Profile**（缓存/存储在 AppData 下）
- **旧数据自动迁移**（首次启动把程序目录下的旧 bookmarks/history 搬到 AppData）

## 数据存储位置

所有用户数据写入 **`QStandardPaths::AppDataLocation`**（即
`C:\Users\<用户>\AppData\Roaming\Breeze\Breeze\`），而非程序目录
（避免 Program Files 下无写权限导致静默失败）：

| 文件 | 内容 |
|------|------|
| `bookmarks.json` | 书签（含分组） |
| `history.json` | 历史记录 |
| `downloads.json` | 下载记录元数据 |
| `userscripts.json` | 用户脚本 / 用户样式 |
| `profile/` | WebEngine 持久化存储 |
| `profile/cache/` | WebEngine 缓存 |

会话恢复数据存于 `QSettings`（组织名/应用名均为 `Breeze`，注册表或 ini）。

下载文件默认保存到系统「下载」目录（`QStandardPaths::DownloadLocation`）。

## 构建方式

### 方式一：使用 configure.bat（推荐）

```bat
cd D:\C++\Breeze
configure.bat
cmake --build build
```

`configure.bat` 内容为：

```bat
@echo off
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
set PATH=D:\Qt\6.11.2\msvc2022_64\bin;D:\Qt\Tools\Ninja;%PATH%
set CMAKE_PREFIX_PATH=D:\Qt\6.11.2\msvc2022_64
cd /d D:\C++\Breeze
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:\Qt\6.11.2\msvc2022_64
```

> 注意：`cmake --build build` 也必须在同一个 vcvars64 环境里执行。若在普通
> cmd 里单独跑会失败。推荐用一条命令完成：
> ```bat
> cmd /c "call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build D:\C++\Breeze\build"
> ```

### 方式二：手动配置

```bat
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:\Qt\6.11.2\msvc2022_64
cmake --build build
```

### 运行

构建产物位于 `build/Breeze.exe`。运行前需确保 Qt 的 DLL 可被找到，推荐：

```bat
set PATH=D:\Qt\6.11.2\msvc2022_64\bin;%PATH%
build\Breeze.exe
```

或使用 `windeployqt` 拷贝依赖（**只读方式部署，程序目录无写权限也能跑**）：

```bat
D:\Qt\6.11.2\msvc2022_64\bin\windeployqt.exe --release --no-translations build\Breeze.exe
```

## 代码结构说明

### main.cpp

- 构造 `QApplication`，设置应用名 / 版本 / 组织名
- 配置默认 `QWebEngineProfile` 的持久化存储与缓存路径（AppData 下）
- 创建并显示 `BrowserWindow`

### BrowserWindow（browserwindow.h / .cpp）

主窗口，继承 `QMainWindow`。

关键成员：

- `QTabWidget *m_tabs` —— 标签容器
- `QLineEdit *m_urlBar` —— 地址栏
- `QProgressBar *m_progress` —— 加载进度
- `QToolBar *m_bookmarkBar` —— 书签栏
- `QList<Bookmark> m_bookmarks` / `QList<HistoryEntry> m_history` —— 数据
- `DownloadManager *m_downloadManager` / `HistoryDialog *m_historyDialog`

关键方法：

- `createTabView()` —— 建 WebView、连接信号、加入标签栏，并注入"新窗口→新标签"回调
- `createTab(url, switchToTab)` —— 基于 createTabView 再加载 URL
- `migrateLegacyData()` —— 首次启动迁移旧数据（构造时最先调用）
- `currentView()` —— 获取当前标签的 `WebView`
- `recordHistory(WebView*)` —— 按**实际加载完成**的标签记录历史（非当前标签）
- `onUrlEntered()` —— 地址栏回车；网址直跳，否则按设置的搜索引擎搜索
- `normalizedUrl()` —— 判断输入是网址还是搜索词
- `updateTabTitle()` / `updateTabUrl()` —— 同步标签标题与地址栏
- `updateNavButtons()` —— 根据历史记录启用/禁用前进后退

### WebView（webview.h / .cpp）

自定义 `QWebEngineView`：

- `setNewTabProvider(std::function<WebView*()>)` —— 注入"提供新标签视图"的回调
- 重写 `createWindow()` —— 新窗口请求时调用 provider，返回一个已加入标签栏的
  空 `WebView`，Qt 会把新窗口内容加载进去，从而**真正在新标签打开**

### DownloadManager（downloadmanager.h / .cpp）

下载管理对话框：`QTableWidget` 显示文件名 / 进度 / 状态 / 操作；
支持暂停/继续、取消、打开文件夹。以下载对象指针为键映射行号，
并在对象 `destroyed` 时清理映射，避免悬空键。

### HistoryDialog（historymanager.h / .cpp）

历史记录对话框：搜索过滤、双击跳转、清空。

### SettingsDialog（settingsdialog.h / .cpp）

设置对话框；配置读写用静态方法（`QSettings`，组织名/应用名均为 `Breeze`），
提供主页、搜索引擎、是否记录历史，以及 `searchUrlTemplate()` 搜索模板。

## 开发约定

- 源文件统一 UTF-8 编码；CMake 中对 MSVC 添加了 `/utf-8` 编译选项
- 界面使用纯代码构建，**不依赖 .ui 文件**
- 使用 `CMAKE_AUTOMOC` 自动处理 Qt moc
- 字符串字面量优先使用 `QStringLiteral` 包装
- **用户数据一律写入 AppData**，不写程序目录
- 导航按钮使用 `QStyle` 标准图标，不依赖外部图片资源

## 构建依赖（CMake）

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets PrintSupport Network)
```

## 已修复的重要问题（历史记录）

以下是开发过程中修复的隐患，供后续维护参考：

1. **历史记错 URL** —— 后台标签加载完成时曾用 `currentView()` 记录，
   会记录成当前标签的 URL。已改为按 `sender()` 定位实际标签。
2. **进度条/导航按钮被后台标签污染** —— `onLoadStarted/Progress/Finished`
   曾不区分标签。已加 `sender() != currentView()` 判断。
3. **数据写在程序目录** —— 书签/历史/Profile 曾用 `applicationDirPath()`，
   在 Program Files 下无写权限会静默失败。已改用 `AppDataLocation`。
4. **下载目录不可写** —— 下载曾存到程序目录。已改用系统「下载」目录。
5. **书签右键菜单误配** —— 曾用 `findChildren` 全量扫描按钮。
   已改用 `widgetForAction()` 精确取按钮。
6. **下载映射悬空键** —— 曾以裸指针为键且不清理。已加 `destroyed` 连接清理。
7. **新窗口请求未处理** —— 曾只发信号不响应 request。已改为重写
   `createWindow()` + provider 回调，真正在新标签打开。
8. **工具栏用 Unicode 字符** —— 已换成 `QStyle` 标准图标。

## 后续可扩展方向

- 历史记录按站点过滤 / 搜索高亮
- 更完整的扩展机制（脚本元数据解析、@run-at 时机）
- 同步口令强度增强（当前为 SHA-256 派生 + 流加密，非军用级）
- 下载多线程分片
- 更多语言支持
