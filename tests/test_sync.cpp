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

static void testMergeIsSymmetric()
{
    const QByteArray a = R"({"bookmarks":[{"title":"A","url":"https://a.com"}]})";
    const QByteArray b = R"({"bookmarks":[{"title":"B","url":"https://b.com"}]})";
    const QJsonObject ab = QJsonDocument::fromJson(SyncMerge::merge(a, b)).object();
    const QJsonObject ba = QJsonDocument::fromJson(SyncMerge::merge(b, a)).object();
    CHECK(ab.value("bookmarks").toArray().size() == 2);
    CHECK(ba.value("bookmarks").toArray().size() == 2);
}

static void testHistorySortsDescending()
{
    const QByteArray local = R"({
        "history": [
            {"url":"https://a.com","time":"2026-01-01T00:00:00"},
            {"url":"https://b.com","time":"2026-03-01T00:00:00"}
        ]
    })";
    const QJsonArray his = QJsonDocument::fromJson(
        SyncMerge::merge(local, QByteArray("{}"))).object()
        .value("history").toArray();
    CHECK(his.size() == 2);
    // 较新的在前
    CHECK(his.first().toObject().value("url").toString() == QString("https://b.com"));
}

// 分片区间计算：与 SegmentedDownload 中 (total + segs - 1) / segs 的切分一致
static void testSegmentRanges()
{
    const qint64 total = 1000;
    const int segs = 4;
    const qint64 chunk = (total + segs - 1) / segs;   // 250
    CHECK(chunk == 250);
    // 各段范围应无重叠、无空隙、覆盖 [0, total-1]
    qint64 covered = 0;
    for (int i = 0; i < segs; ++i) {
        const qint64 start = i * chunk;
        const qint64 end = qMin(total - 1, start + chunk - 1);
        if (start <= end)
            covered += (end - start + 1);
    }
    CHECK(covered == total);
    // 非整除的情形
    const qint64 total2 = 1001;
    const qint64 chunk2 = (total2 + segs - 1) / segs;   // 251
    qint64 covered2 = 0;
    for (int i = 0; i < segs; ++i) {
        const qint64 start = i * chunk2;
        const qint64 end = qMin(total2 - 1, start + chunk2 - 1);
        if (start <= end)
            covered2 += (end - start + 1);
    }
    CHECK(covered2 == total2);
}

int main()
{
    testBookmarksUnion();
    testHistoryKeepsNewest();
    testEmptyInputs();
    testInvalidJson();
    testMergeIsSymmetric();
    testHistorySortsDescending();
    testSegmentRanges();

    if (g_failures == 0) {
        qInfo() << "All sync merge tests passed.";
        return 0;
    }
    qCritical() << g_failures << "test(s) failed.";
    return 1;
}
