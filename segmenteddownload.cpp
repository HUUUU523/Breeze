#include "segmenteddownload.h"

#include <QElapsedTimer>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

SegmentedDownload::SegmentedDownload(const QUrl &url, const QString &savePath,
                                     int segments, QObject *parent)
    : QObject(parent), m_url(url), m_savePath(savePath),
      m_segments(qMax(1, segments))
{
    m_nam = new QNetworkAccessManager(this);
    m_timer = new QElapsedTimer;
    m_timer->start();
}

SegmentedDownload::~SegmentedDownload()
{
    delete m_timer;
}

void SegmentedDownload::start()
{
    // 先发 HEAD 探测大小与 Range 支持
    QNetworkRequest req(m_url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *head = m_nam->head(req);
    connect(head, &QNetworkReply::finished, this, [this, head]() {
        head->deleteLater();
        qint64 size = -1;
        bool ranges = false;
        if (head->error() == QNetworkReply::NoError) {
            size = head->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            const QByteArray ar = head->rawHeader("Accept-Ranges").toLower();
            ranges = (ar.contains("bytes"));
        }
        startSegments(size, ranges);
    });
}

void SegmentedDownload::startSegments(qint64 totalSize, bool supportsRanges)
{
    m_total = totalSize;
    // 打开/预分配文件
    m_file = new QFile(m_savePath, this);
    if (!m_file->open(QIODevice::ReadWrite)) {
        emit finished(false, QStringLiteral("无法写入文件"));
        return;
    }
    if (totalSize > 0)
        m_file->resize(totalSize);

    // 决定分片数：不支持 Range 或大小未知 → 1 段
    int segs = m_segments;
    if (!supportsRanges || totalSize <= 0)
        segs = 1;

    const qint64 chunk = (segs > 0 && totalSize > 0)
        ? (totalSize + segs - 1) / segs : 0;

    m_pending = segs;
    for (int i = 0; i < segs; ++i) {
        QNetworkRequest req(m_url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        qint64 start = 0, end = -1;
        if (segs > 1) {
            start = i * chunk;
            end = qMin(totalSize - 1, start + chunk - 1);
            req.setRawHeader("Range",
                QByteArray("bytes=") + QByteArray::number(start)
                    + QByteArray("-") + QByteArray::number(end));
        }
        QNetworkReply *reply = m_nam->get(req);
        reply->setReadBufferSize(1024 * 1024);
        m_replies.append(reply);
        m_recvPerReply.insert(reply, 0);
        m_expectedPerReply.insert(reply, segs > 1 ? (end - start + 1) : totalSize);

        const qint64 offset = (segs > 1) ? start : 0;
        connect(reply, &QNetworkReply::readyRead, this, [this, reply, offset]() {
            if (m_paused || m_canceled)
                return;
            const QByteArray data = reply->readAll();
            if (data.isEmpty())
                return;
            // 写各自区间（主线程串行，天然线程安全）
            if (m_file->seek(offset + m_recvPerReply.value(reply))) {
                m_file->write(data);
                m_file->flush();
            }
            m_recvPerReply[reply] += data.size();
            m_received += data.size();
            emit progress(m_received, m_total);
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            onSegmentFinished(reply);
        });
    }

    // 速度上报
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        const qint64 ms = m_timer->elapsed();
        if (ms > 0) {
            const qint64 bps = (m_received - m_lastBytes) * 1000 / qMax<qint64>(1, ms);
            emit speed(bps);
        }
        m_lastBytes = m_received;
        m_timer->restart();
    });
    timer->start(1000);
}

void SegmentedDownload::onSegmentFinished(QNetworkReply *reply)
{
    if (m_finished)
        return;
    const auto err = reply->error();
    reply->deleteLater();
    --m_pending;

    if (m_canceled) {
        checkAllDone();
        return;
    }
    if (err != QNetworkReply::NoError
        && err != QNetworkReply::OperationCanceledError) {
        m_finished = true;
        if (m_file) { m_file->close(); }
        emit finished(false, reply->errorString());
        return;
    }
    checkAllDone();
}

void SegmentedDownload::checkAllDone()
{
    if (m_pending > 0)
        return;
    if (m_finished)
        return;
    m_finished = true;
    if (m_file) {
        m_file->flush();
        m_file->close();
    }
    emit progress(m_received, m_total);
    emit finished(!m_canceled, m_canceled ? QStringLiteral("已取消") : QString());
}

void SegmentedDownload::cancel()
{
    m_canceled = true;
    for (QNetworkReply *r : m_replies)
        if (r) r->abort();
}

void SegmentedDownload::pause()
{
    m_paused = true;
    for (QNetworkReply *r : m_replies)
        if (r) r->setReadBufferSize(1);   // 触发流控
}

void SegmentedDownload::resume()
{
    m_paused = false;
    for (QNetworkReply *r : m_replies)
        if (r) r->setReadBufferSize(1024 * 1024);
}
