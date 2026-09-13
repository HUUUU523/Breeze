#include "syncmerge.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace SyncMerge {

QByteArray merge(const QByteArray &local, const QByteArray &remote)
{
    const QJsonObject lo = QJsonDocument::fromJson(local).object();
    const QJsonObject ro = QJsonDocument::fromJson(remote).object();

    QJsonObject out;

    // ---- 书签：按 URL 去重取并集，本地优先 ----
    QJsonArray bmOut;
    QSet<QString> seen;
    auto addBookmarks = [&](const QJsonObject &src) {
        for (const QJsonValue &v : src.value(QStringLiteral("bookmarks")).toArray()) {
            const QJsonObject o = v.toObject();
            const QString url = o.value(QStringLiteral("url")).toString();
            if (url.isEmpty() || seen.contains(url))
                continue;
            seen.insert(url);
            bmOut.append(o);
        }
    };
    addBookmarks(lo);
    addBookmarks(ro);
    out.insert(QStringLiteral("bookmarks"), bmOut);

    // ---- 历史：按 URL 去重，保留较新的时间 ----
    QHash<QString, QJsonObject> hisMap;
    auto addHistory = [&](const QJsonObject &src) {
        for (const QJsonValue &v : src.value(QStringLiteral("history")).toArray()) {
            const QJsonObject o = v.toObject();
            const QString url = o.value(QStringLiteral("url")).toString();
            if (url.isEmpty())
                continue;
            if (!hisMap.contains(url)) {
                hisMap.insert(url, o);
            } else {
                const QDateTime a = QDateTime::fromString(
                    hisMap[url].value(QStringLiteral("time")).toString(), Qt::ISODate);
                const QDateTime b = QDateTime::fromString(
                    o.value(QStringLiteral("time")).toString(), Qt::ISODate);
                if (b > a)
                    hisMap[url] = o;
            }
        }
    };
    addHistory(lo);
    addHistory(ro);
    QJsonArray hisOut;
    for (const QJsonObject &o : hisMap)
        hisOut.append(o);
    out.insert(QStringLiteral("history"), hisOut);

    return QJsonDocument(out).toJson(QJsonDocument::Compact);
}

} // namespace SyncMerge
