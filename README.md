# Breeze

基于 **Qt6 + Qt WebEngine** 的轻量多标签浏览器，C++ 编写，界面极简。

![version](https://img.shields.io/badge/version-2.4.3-blue)
![platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey)
![license](https://img.shields.io/badge/license-MIT-green)

## 特性

- **极简界面**：工具栏只有 `← → ↻ ｜ 地址栏 ｜ + ⋮`，全部功能收进菜单
- **多标签浏览**：支持关闭、拖动排序、会话恢复
- **书签**：分组管理、拖拽整理、JSON 持久化
- **下载**：管理窗口、暂停/继续、断点续传、记录持久化
- **历史**：按日期分组、搜索过滤
- **页面工具**：查找（Ctrl+F）、缩放（Ctrl +/-）、打印、导出 PDF
- **隐私模式**：独立非持久化 Profile，不写盘、不记历史
- **深色主题**：跟随系统 / 手动切换
- **云同步**：WebDAV，书签+历史加密上传下载，双向合并
- **用户脚本**：JS / CSS 注入，URL 通配匹配
- **AI 助手**：OpenAI 兼容接口，多轮对话 + 流式输出 + 网页总结
- **工具箱**：JSON / URL / Base64 / 时间戳 / 密码 / 颜色 / 单位 / 二维码
- **多语言**：中文 / English
- **广告拦截**：内置常见广告/追踪域名黑名单 + URL 关键字规则，可开关
- **右键增强**：网页右键可复制链接、AI 处理选中文字（解释/翻译/改写）
- **导入导出**：书签（Chrome/Edge HTML 格式）、历史（JSON）
- **阅读模式**：提取正文、字号/背景切换（Esc 退出）
- **页面截图**：可视区域截图保存为 PNG
- **标签增强**：固定标签、单标签静音、Ctrl+Shift+T 恢复关闭
- **地址栏补全**：历史 + 书签智能提示
- **代理设置**：HTTP / SOCKS5 全局代理（重启生效）
- **盾牌计数**：地址栏显示当前站点拦截数，点击查看详情
- **划词工具栏**：选中文字浮出「解释 / 翻译 / 搜索」
- **HTTPS 升级**：http 主框架请求强制升级；证书错误可继续访问
- **Cookie 管理**：列出 / 删除 Cookie
- **一键清除**：历史 / 缓存 / Cookie / 下载记录
- **AI 侧边栏**：常驻停靠面板，可带入当前页正文问答
- **自动更新**：检查 GitHub Release 新版本
- **扩展系统**（最小）：加载含 manifest.json 的扩展目录，注入 content scripts

## 截图

> 运行截图待补充

## 系统要求

- **Windows 10/11 x64**
- **Qt 6**（含 WebEngineWidgets、PrintSupport、Network 模块）
- **MSVC 2022**（Qt WebEngine 只提供 msvc 套件，**不支持 MinGW**）
- CMake 3.19+、Ninja

## 构建

> ⚠️ Qt WebEngine 只安装在 msvc 套件中；构建**必须**在 vcvars64 环境下执行。

```bat
call "D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:\Qt\6.11.2\msvc2022_64
cmake --build build
```

或使用附带的 `configure.bat`：

```bat
configure.bat
cmake --build build
```

### 部署依赖

```bat
D:\Qt\6.11.2\msvc2022_64\bin\windeployqt.exe --release build\Breeze.exe
```

### 运行

```bat
build\Breeze.exe
```

## 数据存储

用户数据写入 **`%APPDATA%\Breeze\Breeze\`**：

| 文件 | 内容 |
|------|------|
| `bookmarks.json` | 书签 |
| `history.json` | 历史记录 |
| `downloads.json` | 下载记录 |
| `userscripts.json` | 用户脚本 |
| `extensions.json` | 已加载的扩展目录列表 |
| `logs/breeze.log` | 运行日志 |

## 项目结构

```
Breeze/
├── main.cpp              入口
├── browserwindow.*       主窗口（标签、导航、菜单）
├── webview.*             自定义 QWebEngineView
├── downloadmanager.*     下载管理
├── historymanager.*      历史记录
├── bookmarkmanager.*     书签管理
├── settingsdialog.*      设置
├── syncmanager.*         云同步（WebDAV）
├── syncdialog.*          同步配置
├── userscriptmanager.*   用户脚本 / 样式
├── translator.*          多语言
├── toolbox.*             工具箱
├── qrcodegen.*           二维码生成（内置）
├── aimanager.*           AI 接口（OpenAI 兼容）
├── aidialog.*            AI 对话窗口
├── aisidebar.*           AI 侧边栏
├── adblocker.*           广告拦截（含盾牌计数、HTTPS 升级）
├── cookiemanagerdialog.* Cookie 管理
├── updatemanager.*       GitHub 自动更新检查
├── logger.*              文件日志
├── syncmerge.*           同步数据合并（纯逻辑）
├── extension.*           扩展系统（manifest 解析）
├── extensiondialog.*     扩展管理
└── tests/                单元测试（Qt Test + CTest）
```

## 快捷键

| 快捷键 | 功能 |
|--------|------|
| Ctrl+T | 新建标签页 |
| Ctrl+Shift+N | 新建隐私标签页 |
| Ctrl+W | 关闭标签页 |
| Ctrl+L | 聚焦地址栏 |
| Ctrl+F | 页面查找 |
| F5 / Ctrl+R | 刷新 |
| Alt+← / → | 后退 / 前进 |
| Ctrl+Tab | 切换标签 |
| Ctrl+= / - / 0 | 缩放 |
| Ctrl+P | 打印 |
| Ctrl+Shift+P | 保存为 PDF |
| Ctrl+Shift+T | 恢复关闭的标签 |
| Ctrl+点击 / 中键点击链接 | 后台打开 |

## AI 配置

在「⋮ → AI 对话」中填写接口地址、API Key、模型名。兼容所有 OpenAI 协议服务：

| 服务 | 接口地址 | 模型示例 |
|------|---------|---------|
| DeepSeek | `https://api.deepseek.com/v1/chat/completions` | `deepseek-chat` |
| OpenAI | `https://api.openai.com/v1/chat/completions` | `gpt-4o-mini` |
| 通义千问 | `https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions` | `qwen-plus` |
| Ollama | `http://localhost:11434/v1/chat/completions` | `qwen2.5` |

## 许可证

MIT
