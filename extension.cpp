#include "extension.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString configPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/extensions.json");
}

// 通配匹配：*://*.host/path* → 正则
bool matchPattern(const QString &pattern, const QUrl &url)
{
    QString regex = QRegularExpression::escape(pattern);
    regex.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
    QRegularExpression re(QStringLiteral("^") + regex + QStringLiteral("$"),
                          QRegularExpression::CaseInsensitiveOption);
    return re.match(url.toString()).hasMatch();
}

QStringList toStringList(const QJsonValue &v)
{
    QStringList out;
    if (v.isArray()) {
        for (const QJsonValue &e : v.toArray())
            if (e.isString()) out << e.toString();
    } else if (v.isString()) {
        out << v.toString();
    }
    return out;
}

} // namespace

namespace ExtensionManager {

QStringList loadedDirs()
{
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    return toStringList(o.value(QStringLiteral("dirs")));
}

static void saveDirs(const QStringList &dirs)
{
    QJsonObject o;
    QJsonArray arr;
    for (const QString &d : dirs)
        arr.append(d);
    o.insert(QStringLiteral("dirs"), arr);
    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

void addDir(const QString &dir)
{
    QStringList dirs = loadedDirs();
    const QString norm = QDir::cleanPath(dir);
    if (!dirs.contains(norm))
        dirs << norm;
    saveDirs(dirs);
}

void removeDir(const QString &dir)
{
    QStringList dirs = loadedDirs();
    dirs.removeAll(QDir::cleanPath(dir));
    saveDirs(dirs);
}

Extension loadExtension(const QString &dir)
{
    Extension ext;
    ext.dir = dir;

    QFile f(dir + QStringLiteral("/manifest.json"));
    if (!f.open(QIODevice::ReadOnly))
        return ext;

    const QJsonObject m = QJsonDocument::fromJson(f.readAll()).object();
    ext.name = m.value(QStringLiteral("name")).toString(QDir(dir).dirName());
    ext.version = m.value(QStringLiteral("version")).toString();

    const QJsonArray csArr = m.value(QStringLiteral("content_scripts")).toArray();
    for (const QJsonValue &v : csArr) {
        const QJsonObject o = v.toObject();
        ContentScript cs;
        cs.matches = toStringList(o.value(QStringLiteral("matches")));
        cs.js      = toStringList(o.value(QStringLiteral("js")));
        cs.css     = toStringList(o.value(QStringLiteral("css")));
        const QString ra = o.value(QStringLiteral("run_at")).toString();
        if (!ra.isEmpty())
            cs.runAt = ra;
        ext.contentScripts.append(cs);
    }

    // 解析 background：支持 service_worker（单文件）或 scripts（数组）
    const QJsonObject bg = m.value(QStringLiteral("background")).toObject();
    if (bg.contains(QStringLiteral("service_worker"))) {
        const QString sw = bg.value(QStringLiteral("service_worker")).toString();
        if (!sw.isEmpty())
            ext.backgroundScripts << sw;
    }
    ext.backgroundScripts += toStringList(bg.value(QStringLiteral("scripts")));

    // 解析 action（MV3）或 browser_action（MV2）
    QJsonObject act = m.value(QStringLiteral("action")).toObject();
    if (act.isEmpty())
        act = m.value(QStringLiteral("browser_action")).toObject();
    if (!act.isEmpty()) {
        ext.hasAction = true;
        ext.actionTitle = act.value(QStringLiteral("default_title")).toString();
        ext.actionPopup = act.value(QStringLiteral("default_popup")).toString();
        // default_icon 可能是字符串，也可能是 {size: path} 对象
        const QJsonValue iconVal = act.value(QStringLiteral("default_icon"));
        if (iconVal.isString())
            ext.actionIcon = iconVal.toString();
        else if (iconVal.isObject()) {
            const QJsonObject icons = iconVal.toObject();
            // 取第一个（或最小的）图标
            for (auto it = icons.constBegin(); it != icons.constEnd(); ++it) {
                if (it.value().isString()) { ext.actionIcon = it.value().toString(); break; }
            }
        }
    }

    return ext;
}

QList<Extension> loadAll()
{
    QList<Extension> out;
    for (const QString &dir : loadedDirs()) {
        Extension e = loadExtension(dir);
        if (!e.name.isEmpty())
            out.append(e);
    }
    return out;
}

bool matchesUrl(const ContentScript &cs, const QUrl &url)
{
    for (const QString &p : cs.matches) {
        if (p == QStringLiteral("<all_urls>")) return true;
        if (matchPattern(p, url)) return true;
    }
    return false;
}

} // namespace ExtensionManager
