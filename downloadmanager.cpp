#include "downloadmanager.h"
#include "profilemanager.h"

#include <QApplication>
#include <QClipboard>
#include <QElapsedTimer>
#include <memory>
#include <QSettings>
#include <QThread>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QStyle>
#include "settingsdialog.h"
#include "segmenteddownload.h"

#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>

DownloadManager::DownloadManager(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("下载管理 - Breeze"));
    resize(760, 420);

    auto *layout = new QVBoxLayout(this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("文件名"),
        QStringLiteral("进度"),
        QStringLiteral("状态"),
        QStringLiteral("操作")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 160);
    m_table->setColumnWidth(2, 90);
    m_table->setColumnWidth(3, 200);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);

    // 双击已完成的下载 → 用系统默认程序打开
    connect(m_table, &QTableWidget::itemDoubleClicked, this,
            [this](QTableWidgetItem *item) {
                if (!item)
                    return;
                const int row = item->row();
                auto *statusItem = m_table->item(row, 2);
                if (!statusItem || statusItem->text() != QStringLiteral("已完成"))
                    return;
                auto *pathItem = m_table->item(row, 3);
                if (!pathItem)
                    return;
                const QString path = pathItem->text();
                if (!QFile::exists(path))
                    return;
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            });

    // 右键菜单：优先下载 / 复制地址
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                auto *item = m_table->itemAt(pos);
                if (!item)
                    return;
                const int row = item->row();
                QMenu menu(this);
                QAction *prio = menu.addAction(QStringLiteral("优先下载（置顶队列）"));
                QAction *copyPath = menu.addAction(QStringLiteral("复制保存路径"));
                QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
                if (chosen == copyPath) {
                    if (auto *p = m_table->item(row, 3))
                        QApplication::clipboard()->setText(p->text());
                    return;
                }
                if (chosen == prio) {
                    // 找到该行对应的待下载任务，移到队列最前
                    auto *pathItem = m_table->item(row, 3);
                    if (!pathItem)
                        return;
                    const QString savePath = pathItem->text();
                    for (int i = 0; i < m_queue.size(); ++i) {
                        if (m_queue.at(i).savePath == savePath) {
                            const PendingJob job = m_queue.takeAt(i);
                            m_queue.prepend(job);
                            break;
                        }
                    }
                    scheduleNext();
                }
            });

    layout->addWidget(m_table);

    auto *pauseAllBtn = new QPushButton(QStringLiteral("全部暂停"), this);
    auto *resumeAllBtn = new QPushButton(QStringLiteral("全部继续"), this);
    auto *clearBtn = new QPushButton(QStringLiteral("清空已完成记录"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    connect(pauseAllBtn, &QPushButton::clicked, this, [this]() {
        for (auto it = m_rows.constBegin(); it != m_rows.constEnd(); ++it) {
            if (it.key() && it.key()->state() == QWebEngineDownloadRequest::DownloadInProgress)
                it.key()->pause();
        }
    });
    connect(resumeAllBtn, &QPushButton::clicked, this, [this]() {
        for (auto it = m_rows.constBegin(); it != m_rows.constEnd(); ++it) {
            if (it.key() && it.key()->isPaused())
                it.key()->resume();
        }
    });
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, QStringLiteral("清空记录"),
                QStringLiteral("清空已完成/已取消的下载记录？（不影响已下载的文件）"))
            != QMessageBox::Yes)
            return;
        for (int i = m_records.size() - 1; i >= 0; --i) {
            const QString st = m_records.at(i).status;
            if (st == QStringLiteral("已完成") || st == QStringLiteral("已取消")
                || st == QStringLiteral("已中断"))
                m_records.removeAt(i);
        }
        saveRecords();
        // 重载表格显示
        m_table->setRowCount(0);
        for (const DownloadRecord &r : m_records) {
            const int row = m_table->rowCount();
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(r.fileName));
            auto *bar = new QProgressBar(m_table);
            bar->setRange(0, 100);
            bar->setValue(100);
            m_table->setCellWidget(row, 1, bar);
            m_table->setItem(row, 2, new QTableWidgetItem(r.status));
            auto *btn = new QPushButton(QStringLiteral("文件夹"), m_table);
            const QString dir = r.directory;
            connect(btn, &QPushButton::clicked, this, [dir]() {
                QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
            });
            m_table->setCellWidget(row, 3, btn);
        }
    });
    auto *bottom = new QHBoxLayout;
    bottom->addWidget(pauseAllBtn);
    bottom->addWidget(resumeAllBtn);
    bottom->addWidget(clearBtn);
    bottom->addStretch();
    bottom->addWidget(closeBtn);
    layout->addLayout(bottom);

    m_net = new QNetworkAccessManager(this);

    // "新建下载"按钮
    auto *newBtn = new QPushButton(QStringLiteral("新建下载（支持续传）"), this);
    connect(newBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString url = QInputDialog::getText(
            this, QStringLiteral("新建下载"), QStringLiteral("URL："),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || url.trimmed().isEmpty())
            return;
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        const QString save = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存到"), dir);
        if (save.isEmpty())
            return;
        enqueueDownload(QUrl(url.trimmed()), save);
    });
    layout->addWidget(newBtn);

    loadRecords();
}

int DownloadManager::rowForDownload(QWebEngineDownloadRequest *download) const
{
    return m_rows.value(download, -1);
}

void DownloadManager::enqueueDownload(const QUrl &url, const QString &savePath)
{
    // 先在表格里占一行"排队中"，让用户看到队列
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_table->setItem(row, 0, new QTableWidgetItem(QFileInfo(savePath).fileName()));
    auto *bar = new QProgressBar(m_table);
    bar->setRange(0, 100);
    bar->setValue(0);
    m_table->setCellWidget(row, 1, bar);
    m_table->setItem(row, 2, new QTableWidgetItem(QStringLiteral("排队中")));
    m_table->setItem(row, 3, new QTableWidgetItem(savePath));

    m_queue.append({url, savePath});
    scheduleNext();
}

void DownloadManager::scheduleNext()
{
    while (m_activeCount < m_maxConcurrent && !m_queue.isEmpty()) {
        const PendingJob job = m_queue.takeFirst();
        ++m_activeCount;
        // 移除对应的"排队中"行（按保存路径匹配，取最后一条）
        for (int r = m_table->rowCount() - 1; r >= 0; --r) {
            auto *p = m_table->item(r, 3);
            auto *s = m_table->item(r, 2);
            if (p && s && s->text() == QStringLiteral("排队中")
                && p->text() == job.savePath) {
                m_table->removeRow(r);
                break;
            }
        }
        startResumableDownload(job.url, job.savePath);
    }
}

void DownloadManager::startResumableDownload(const QUrl &url, const QString &savePath)
{
    // 已有部分文件则从其大小处续传
    qint64 existing = 0;
    QFile f(savePath);
    if (f.exists())
        existing = f.size();

    // 全新下载：交给多线程分片下载器（内部会 HEAD 探测，必要时回退单段）
    if (existing == 0) {
        startSegmentedDownload(url, savePath);
        return;
    }

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    if (existing > 0)
        req.setRawHeader("Range", QByteArray("bytes=") + QByteArray::number(existing) + "-");

    QNetworkReply *reply = m_net->get(req);
    // 增大读缓冲，减少小包写盘次数（对大文件更友好）
    reply->setReadBufferSize(1024 * 1024);

    // 表格行
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    const QString name = QFileInfo(savePath).fileName();
    m_table->setItem(row, 0, new QTableWidgetItem(name));
    auto *bar = new QProgressBar(m_table);
    bar->setRange(0, 100);
    m_table->setCellWidget(row, 1, bar);
    m_table->setItem(row, 2, new QTableWidgetItem(
        existing > 0 ? QStringLiteral("续传中") : QStringLiteral("下载中")));
    m_table->setItem(row, 3, new QTableWidgetItem(savePath));

    // 打开文件（续传则 append）
    auto *out = new QFile(savePath, this);
    if (!out->open(QIODevice::WriteOnly | (existing > 0 ? QIODevice::Append : QIODevice::Truncate))) {
        QMessageBox::warning(this, QStringLiteral("Breeze"),
                             QStringLiteral("无法写入文件：") + savePath);
        out->deleteLater();
        reply->deleteLater();
        return;
    }

    // 限速：KB/s，0 = 不限速
    const int limitKB = QSettings(QStringLiteral("Breeze"), ProfileManager::settingsAppName())
                            .value(QStringLiteral("download/speedLimitKB"), 0).toInt();

    connect(reply, &QNetworkReply::readyRead, this, [reply, out, limitKB]() {
        const QByteArray data = reply->readAll();
        out->write(data);
        out->flush();
        if (limitKB > 0 && !data.isEmpty()) {
            // 按本次数据量对应的时长节流（毫秒）
            const int ms = (data.size() * 1000) / (limitKB * 1024);
            if (ms > 0)
                QThread::msleep(static_cast<unsigned long>(qMin(ms, 1000)));
        }
    });

    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, reply, bar, existing, row, elapsed](qint64 recv, qint64 total) {
                if (total > 0)
                    bar->setValue(static_cast<int>((existing + recv) * 100 / (existing + total)));
                // 状态列显示速度
                const qint64 ms = elapsed->elapsed();
                if (ms > 500 && recv > 0) {
                    const double kbPerSec = double(recv) / (ms / 1000.0) / 1024.0;
                    QString speed;
                    if (kbPerSec >= 1024)
                        speed = QStringLiteral("%1 MB/s").arg(kbPerSec / 1024, 0, 'f', 1);
                    else
                        speed = QStringLiteral("%1 KB/s").arg(kbPerSec, 0, 'f', 0);
                    if (auto *item = m_table->item(row, 2)) {
                        if (item->text() == QStringLiteral("下载中")
                            || item->text() == QStringLiteral("续传中")
                            || item->text().contains(QStringLiteral("/s")))
                            item->setText(speed);
                    }
                }
            });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, out, savePath, name, url, existing]() {
                out->close();
                out->deleteLater();
                const bool ok = (reply->error() == QNetworkReply::NoError);
                const bool canceled =
                    (reply->error() == QNetworkReply::OperationCanceledError);
                reply->deleteLater();

                // 中断（非取消、非完成）时自动重试续传，最多 3 次
                if (!ok && !canceled) {
                    const int tries = m_retryCount.value(savePath, 0);
                    if (tries < 3) {
                        m_retryCount.insert(savePath, tries + 1);
                        // 稍后重试（利用已有文件续传）
                        QTimer::singleShot(1500, this, [this, url, savePath]() {
                            startResumableDownload(url, savePath);
                        });
                        return;
                    }
                    m_retryCount.remove(savePath);
                }

                DownloadRecord rec;
                rec.fileName = name;
                rec.directory = QFileInfo(savePath).absolutePath();
                rec.totalBytes = QFileInfo(savePath).size();
                rec.startedAt = QDateTime::currentDateTime();
                rec.status = ok ? QStringLiteral("已完成") : QStringLiteral("已中断");
                appendRecord(rec);

                if (!ok)
                    qWarning("下载中断：%s", qPrintable(name));

                // 一个任务结束，调度队列中的下一个
                if (m_activeCount > 0)
                    --m_activeCount;
                scheduleNext();
            });
}

void DownloadManager::startSegmentedDownload(const QUrl &url, const QString &savePath)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    const QString name = QFileInfo(savePath).fileName();
    m_table->setItem(row, 0, new QTableWidgetItem(name));
    auto *bar = new QProgressBar(m_table);
    bar->setRange(0, 100);
    m_table->setCellWidget(row, 1, bar);
    m_table->setItem(row, 2, new QTableWidgetItem(QStringLiteral("分片下载中")));
    m_table->setItem(row, 3, new QTableWidgetItem(savePath));

    auto *seg = new SegmentedDownload(url, savePath, 4, this);
    connect(seg, &SegmentedDownload::progress, this,
            [bar](qint64 recv, qint64 total) {
                if (total > 0)
                    bar->setValue(static_cast<int>(recv * 100 / total));
            });
    connect(seg, &SegmentedDownload::speed, this,
            [this, row](qint64 bps) {
                const double kb = bps / 1024.0;
                if (auto *item = m_table->item(row, 2)) {
                    item->setText(kb >= 1024
                        ? QStringLiteral("%1 MB/s").arg(kb / 1024, 0, 'f', 1)
                        : QStringLiteral("%1 KB/s").arg(kb, 0, 'f', 0));
                }
            });
    connect(seg, &SegmentedDownload::finished, this,
            [this, seg, savePath, name](bool ok, const QString &err) {
                DownloadRecord rec;
                rec.fileName = name;
                rec.directory = QFileInfo(savePath).absolutePath();
                rec.totalBytes = QFileInfo(savePath).size();
                rec.startedAt = QDateTime::currentDateTime();
                rec.status = ok ? QStringLiteral("已完成") : QStringLiteral("已中断");
                appendRecord(rec);

                if (!ok && !err.isEmpty())
                    qWarning("分片下载失败：%s", qPrintable(err));
                seg->deleteLater();

                // 一个任务结束，调度队列中的下一个
                if (m_activeCount > 0)
                    --m_activeCount;
                scheduleNext();
            });
    seg->start();
}



QString DownloadManager::recordsFilePath() const
{
    return ProfileManager::dataDir() + QStringLiteral("/downloads.json");
}

void DownloadManager::loadRecords()
{
    m_records.clear();
    QFile f(recordsFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        DownloadRecord r;
        r.fileName   = o.value(QStringLiteral("fileName")).toString();
        r.directory  = o.value(QStringLiteral("directory")).toString();
        r.status     = o.value(QStringLiteral("status")).toString();
        r.totalBytes = static_cast<qint64>(o.value(QStringLiteral("totalBytes")).toDouble());
        r.startedAt  = QDateTime::fromString(
            o.value(QStringLiteral("startedAt")).toString(), Qt::ISODate);
        if (!r.fileName.isEmpty())
            m_records.append(r);
    }

    // 把历史记录显示到表格（无操作按钮，仅展示）
    for (const DownloadRecord &r : m_records) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *nameItem = new QTableWidgetItem(r.fileName);
        nameItem->setToolTip(r.directory);
        m_table->setItem(row, 0, nameItem);

        auto *bar = new QProgressBar(m_table);
        bar->setRange(0, 100);
        bar->setValue(100);
        m_table->setCellWidget(row, 1, bar);

        m_table->setItem(row, 2, new QTableWidgetItem(r.status));

        auto *openDirBtn = new QPushButton(QStringLiteral("文件夹"), m_table);
        const QString fullPath = r.directory + QLatin1Char('/') + r.fileName;
        const QString dir = r.directory;
        connect(openDirBtn, &QPushButton::clicked, this, [fullPath, dir]() {
#ifdef Q_OS_WIN
            if (QFile::exists(fullPath))
                QProcess::startDetached(QStringLiteral("explorer.exe"),
                                        { QStringLiteral("/select,"), QDir::toNativeSeparators(fullPath) });
            else
                QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
#else
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
#endif
        });
        m_table->setCellWidget(row, 3, openDirBtn);
    }
}

void DownloadManager::saveRecords() const
{
    QJsonArray arr;
    for (const DownloadRecord &r : m_records) {
        QJsonObject o;
        o.insert(QStringLiteral("fileName"), r.fileName);
        o.insert(QStringLiteral("directory"), r.directory);
        o.insert(QStringLiteral("status"), r.status);
        o.insert(QStringLiteral("totalBytes"), static_cast<double>(r.totalBytes));
        o.insert(QStringLiteral("startedAt"), r.startedAt.toString(Qt::ISODate));
        arr.append(o);
    }
    QFile f(recordsFilePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
        f.close();
    }
}

void DownloadManager::appendRecord(const DownloadRecord &rec)
{
    // 同名同目录视为同一条，更新之
    for (DownloadRecord &r : m_records) {
        if (r.fileName == rec.fileName && r.directory == rec.directory) {
            r = rec;
            saveRecords();
            return;
        }
    }
    m_records.prepend(rec);
    if (m_records.size() > 200)
        m_records = m_records.mid(0, 200);
    saveRecords();
    updateWindowTitle();
}

void DownloadManager::updateWindowTitle()
{
    int active = 0;
    for (auto it = m_rows.constBegin(); it != m_rows.constEnd(); ++it) {
        if (it.key() && it.key()->state() == QWebEngineDownloadRequest::DownloadInProgress)
            ++active;
    }
    setWindowTitle(active > 0
        ? QStringLiteral("下载管理 - Breeze（%1 个进行中）").arg(active)
        : QStringLiteral("下载管理 - Breeze"));
}

void DownloadManager::updateRecord(QWebEngineDownloadRequest *download)
{
    const int row = rowForDownload(download);
    if (row < 0)
        return;
    // 只更新当前会话新增记录（前 m_rows.size() 条）
    DownloadRecord rec;
    rec.fileName = download->downloadFileName();
    rec.directory = download->downloadDirectory();
    rec.totalBytes = download->totalBytes();
    rec.startedAt = QDateTime::currentDateTime();

    switch (download->state()) {
    case QWebEngineDownloadRequest::DownloadCompleted:
        rec.status = QStringLiteral("已完成");
        if (SettingsDialog::openFileAfterDownload()) {
            const QString path = download->downloadDirectory()
                + QLatin1Char('/') + download->downloadFileName();
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
        break;
    case QWebEngineDownloadRequest::DownloadCancelled:  rec.status = QStringLiteral("已取消"); break;
    case QWebEngineDownloadRequest::DownloadInterrupted: rec.status = QStringLiteral("已中断"); break;
    default: rec.status = QStringLiteral("下载中"); break;
    }

    // 从会话记录里找已存在的同项更新，否则追加
    appendRecord(rec);
}

void DownloadManager::addDownload(QWebEngineDownloadRequest *download)
{
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    m_rows.insert(download, row);

    // 文件名
    auto *nameItem = new QTableWidgetItem(download->downloadFileName());
    nameItem->setToolTip(download->downloadDirectory());
    m_table->setItem(row, 0, nameItem);

    // 进度条
    auto *bar = new QProgressBar(m_table);
    bar->setRange(0, 100);
    bar->setValue(0);
    m_table->setCellWidget(row, 1, bar);

    // 状态
    m_table->setItem(row, 2, new QTableWidgetItem(QStringLiteral("下载中")));

    // 操作按钮
    auto *btnWidget = new QWidget(m_table);
    auto *btnLayout = new QHBoxLayout(btnWidget);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    auto *pauseBtn = new QPushButton(QStringLiteral("暂停"), btnWidget);
    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), btnWidget);
    auto *openDirBtn = new QPushButton(QStringLiteral("文件夹"), btnWidget);
    auto *copyUrlBtn = new QPushButton(QStringLiteral("链接"), btnWidget);
    auto *retryBtn  = new QPushButton(QStringLiteral("重试"), btnWidget);
    btnLayout->addWidget(pauseBtn);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(retryBtn);
    btnLayout->addWidget(openDirBtn);
    btnLayout->addWidget(copyUrlBtn);
    connect(copyUrlBtn, &QPushButton::clicked, this, [download]() {
        QApplication::clipboard()->setText(download->url().toString());
    });
    connect(retryBtn, &QPushButton::clicked, this, [this, download]() {
        const QUrl u = download->url();
        const QString dir = download->downloadDirectory();
        const QString name = download->downloadFileName();
        if (!u.isValid() || dir.isEmpty() || name.isEmpty())
            return;
        enqueueDownload(u, dir + QLatin1Char('/') + name);
    });
    m_table->setCellWidget(row, 3, btnWidget);

    connect(pauseBtn, &QPushButton::clicked, this, [download, pauseBtn]() {
        if (download->isPaused()) {
            download->resume();
            pauseBtn->setText(QStringLiteral("暂停"));
        } else {
            download->pause();
            pauseBtn->setText(QStringLiteral("继续"));
        }
    });

    connect(cancelBtn, &QPushButton::clicked, this, [this, download]() {
        download->cancel();
        const int r = rowForDownload(download);
        if (r >= 0)
            m_table->item(r, 2)->setText(QStringLiteral("已取消"));
        updateRecord(download);
    });

    connect(openDirBtn, &QPushButton::clicked, this, [this, download]() {
        const QString path = download->downloadDirectory()
            + QLatin1Char('/') + download->downloadFileName();
#ifdef Q_OS_WIN
        QProcess::startDetached(QStringLiteral("explorer.exe"),
                                { QStringLiteral("/select,"), QDir::toNativeSeparators(path) });
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(download->downloadDirectory()));
#endif
    });

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this,
            [this, download, bar]() {
                const qint64 total = download->totalBytes();
                if (total > 0) {
                    const int pct = static_cast<int>(download->receivedBytes() * 100 / total);
                    bar->setValue(pct);
                }
            });

    connect(download, &QWebEngineDownloadRequest::stateChanged, this,
            &DownloadManager::onStateChanged);

    // 下载对象销毁时移除映射，避免 m_rows 残留悬空指针键
    connect(download, &QObject::destroyed, this, [this, download]() {
        m_rows.remove(download);
    });

    show();
    raise();
    activateWindow();
}

void DownloadManager::onStateChanged()
{
    auto *download = qobject_cast<QWebEngineDownloadRequest *>(sender());
    if (!download)
        return;
    const int row = rowForDownload(download);
    if (row < 0)
        return;

    QString stateText;
    switch (download->state()) {
    case QWebEngineDownloadRequest::DownloadRequested: stateText = QStringLiteral("等待中"); break;
    case QWebEngineDownloadRequest::DownloadInProgress:
        stateText = download->isPaused() ? QStringLiteral("已暂停") : QStringLiteral("下载中");
        break;
    case QWebEngineDownloadRequest::DownloadCompleted:  stateText = QStringLiteral("已完成"); break;
    case QWebEngineDownloadRequest::DownloadCancelled:  stateText = QStringLiteral("已取消"); break;
    case QWebEngineDownloadRequest::DownloadInterrupted: stateText = QStringLiteral("已中断"); break;
    }
    if (auto *item = m_table->item(row, 2))
        item->setText(stateText);

    if (download->state() == QWebEngineDownloadRequest::DownloadCompleted) {
        if (auto *bar = qobject_cast<QProgressBar *>(m_table->cellWidget(row, 1)))
            bar->setValue(100);

        // 系统通知：下载完成
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            auto *tray = new QSystemTrayIcon(this);
            tray->setIcon(qApp->style()->standardIcon(QStyle::SP_ArrowDown));
            tray->show();
            tray->showMessage(QStringLiteral("下载完成"),
                              download->downloadFileName(),
                              QSystemTrayIcon::Information, 3000);
            // 3 秒后清理托盘图标
            QTimer::singleShot(4000, tray, [tray]() {
                tray->hide();
                tray->deleteLater();
            });
        }
    }

    // 状态变化即持久化
    updateRecord(download);
    updateWindowTitle();
}
