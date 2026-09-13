#ifndef EXTENSION_H
#define EXTENSION_H

#include <QList>
#include <QString>
#include <QStringList>

class QUrl;

// 扩展的一个 content script 声明
struct ContentScript {
    QStringList matches;   // URL 匹配模式
    QStringList js;        // 相对扩展目录的 JS 文件路径
    QStringList css;       // 相对扩展目录的 CSS 文件路径
    QString     runAt = QStringLiteral("document_idle");  // document_start / document_end / document_idle
};

// 一个已加载的扩展（最小模型：仅 manifest + content_scripts）
struct Extension {
    QString name;
    QString version;
    QString dir;           // 扩展根目录（绝对路径）
    QList<ContentScript> contentScripts;
    bool enabled = true;
};

// 扩展管理（纯逻辑 + JSON 持久化，UI 无关）
namespace ExtensionManager {

// 已加载的扩展目录列表（持久化于 AppData/extensions.json）
QStringList loadedDirs();

// 加载/卸载扩展目录（仅记录目录，不解析）
void addDir(const QString &dir);
void removeDir(const QString &dir);

// 读取某个扩展目录的 manifest.json 并解析
Extension loadExtension(const QString &dir);

// 加载所有已记录的扩展（跳过解析失败的）
QList<Extension> loadAll();

// URL 匹配（复用与 UserScript 相同的通配规则）
bool matchesUrl(const ContentScript &cs, const QUrl &url);

} // namespace ExtensionManager

#endif // EXTENSION_H
