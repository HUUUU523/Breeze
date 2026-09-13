#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// 检查 GitHub Release 是否有新版本
class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);

    // 发起一次检查（异步）
    void checkForUpdates();

signals:
    // 有新版本可用：version 为最新版本号，pageUrl 为发布页，notes 为更新说明
    void updateAvailable(const QString &version, const QString &pageUrl, const QString &notes);
    // 已是最新
    void upToDate(const QString &currentVersion);
    // 检查失败
    void checkFailed(const QString &error);

private:
    QNetworkAccessManager *m_net = nullptr;
};

#endif // UPDATEMANAGER_H
