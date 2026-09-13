#include <QtTest/QtTest>
#include "syncmerge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// 纯函数式测试：不用 Q_OBJECT / moc，直接手动断言。
static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        qCritical() << "FAIL:" << #cond << "at" << __FILE__ << __LINE__; \
        ++g_failures; \
    } \
} while (0)

static void testBookmarksUnion()
{
    const QByteArray local = R"({
        "bookmarks": [
            {"title":"A-local","url":"https://a.com"},
            {"title":"B","url":"https://b.com"}
        ]
    })";
    const QByteArray remote = R"({
        "bookmarks": [
            {"title":"A-remote","url":"https://a.com"},
            {"title":"C","url":"https://c.com"}
        ]
    })";
    const QJsonObject out = QJsonDocument::fromJson(
        SyncMerge::merge(local, remote)).object();
    const QJsonArray bm = out.value("bookmarks").toArray();

    CHECK(bm.size() == 3);
    bool foundA = false;
    for (const QJsonValue &v : bm) {
        const QJsonObject o = v.toObject();
        if (o.value("url").toString() == "https://a.com") {
            CHECK(o.value("title").toString() == QString("A-local"));
            foundA = true;
        }
    }
    CHECK(foundA);
}

static void testHistoryKeepsNewest()
{
    const QByteArray local = R"({
        "history": [
            {"url":"https://a.com","time":"2026-01-01T00:00:00","title":"old"}
        ]
    })";
    const QByteArray remote = R"({
        "history": [
            {"url":"https://a.com","time":"2026-06-01T00:00:00","title":"new"}
        ]
    })";
    const QJsonObject out = QJsonDocument::fromJson(
        SyncMerge::merge(local, remote)).object();
    const QJsonArray his = out.value("history").toArray();

    CHECK(his.size() == 1);
    CHECK(his.first().toObject().value("title").toString() == QString("new"));
}

static void testEmptyInputs()
{
    const QJsonObject out = QJsonDocument::fromJson(
        SyncMerge::merge(QByteArray("{}"), QByteArray("{}"))).object();
    CHECK(out.contains("bookmarks"));
    CHECK(out.contains("history"));
    CHECK(out.value("bookmarks").toArray().size() == 0);
}

static void testInvalidJson()
{
    const QJsonObject out = QJsonDocument::fromJson(
        SyncMerge::merge(QByteArray("not json"), QByteArray("{}"))).object();
    CHECK(out.contains("bookmarks"));
}

int main()
{
    testBookmarksUnion();
    testHistoryKeepsNewest();
    testEmptyInputs();
    testInvalidJson();

    if (g_failures == 0) {
        qInfo() << "All sync merge tests passed.";
        return 0;
    }
    qCritical() << g_failures << "test(s) failed.";
    return 1;
}
