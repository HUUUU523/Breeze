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
