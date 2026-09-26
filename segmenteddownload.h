#ifndef SEGMENTEDDOWNLOAD_H
#define SEGMENTEDDOWNLOAD_H

#include <QObject>
#include <QUrl>
#include <QList>
#include <QHash>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

// 多线程分片下载器：HEAD 探测 → 多段 Range 并发 → 写同一文件的不同 offset。
// 不支持分片（无 Accept-Ranges / 无 Content-Length）时回退为单段。
class SegmentedDownload : public QObject
{
    Q_OBJECT
public:
    explicit SegmentedDownload(const QUrl &url, const QString &savePath,
                               int segments = 4, QObject *parent = nullptr);
    ~SegmentedDownload() override;

    void start();
    void cancel();
    bool isPaused() const { return m_paused; }
    void pause();
    void resume();

signals:
    // 已接收 / 总大小（总大小为 -1 表示未知）
    void progress(qint64 received, qint64 total);
    void finished(bool ok, const QString &error);
    void speed(qint64 bytesPerSec);

private:
    void startSegments(qint64 totalSize, bool supportsRanges);
    void onSegmentReadyRead(QNetworkReply *reply);
    void onSegmentFinished(QNetworkReply *reply);
    void checkAllDone();

    QUrl m_url;
    QString m_savePath;
    int m_segments;
    QNetworkAccessManager *m_nam = nullptr;
    QFile *m_file = nullptr;
    QList<QNetworkReply *> m_replies;
    QHash<QNetworkReply *, qint64> m_recvPerReply;   // 每段累计接收
    QHash<QNetworkReply *, qint64> m_expectedPerReply;
    qint64 m_total = -1;
    qint64 m_received = 0;
    int m_pending = 0;
    bool m_paused = false;
    bool m_canceled = false;
    bool m_finished = false;
    qint64 m_lastBytes = 0;
    class QElapsedTimer *m_timer = nullptr;
};

#endif // SEGMENTEDDOWNLOAD_H
