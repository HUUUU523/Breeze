#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QDialog>
#include <QHash>
#include <QList>
#include <QDateTime>
#include <QUrl>

class QNetworkAccessManager;
class QFile;

class QTableWidget;
class QWebEngineDownloadRequest;

// 一条已记录的下载（含历史记录，用于持久化）
struct DownloadRecord {
    QString   fileName;
    QString   directory;
    QString   status;        // 下载中/已完成/已取消/已中断
    qint64    totalBytes = 0;
    QDateTime startedAt;
};

// 下载管理窗口：显示下载列表，支持暂停/继续、取消、打开文件夹
class DownloadManager : public QDialog
{
    Q_OBJECT

public:
    explicit DownloadManager(QWidget *parent = nullptr);

    // 注册一个新的下载请求
    void addDownload(QWebEngineDownloadRequest *download);

    // 用 QNetworkAccessManager 手动下载，支持断点续传
    void startResumableDownload(const QUrl &url, const QString &savePath);

private slots:
    void onStateChanged();

private:
    int rowForDownload(QWebEngineDownloadRequest *download) const;

    // 持久化
    void loadRecords();
    void saveRecords() const;
    QString recordsFilePath() const;
    void appendRecord(const DownloadRecord &rec);
    void updateRecord(QWebEngineDownloadRequest *download);

    QTableWidget *m_table = nullptr;
    QHash<QWebEngineDownloadRequest *, int> m_rows;
    QList<DownloadRecord> m_records;
    QNetworkAccessManager *m_net = nullptr;
};

#endif // DOWNLOADMANAGER_H
