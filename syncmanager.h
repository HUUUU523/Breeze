#ifndef SYNCMANAGER_H
#define SYNCMANAGER_H

#include <QByteArray>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// WebDAV 云同步：把本地数据打包加密后上传/下载
class SyncManager : public QObject
{
    Q_OBJECT

public:
    explicit SyncManager(QObject *parent = nullptr);

    // 配置
    void setServer(const QString &url, const QString &user, const QString &password);
    void setRemotePath(const QString &path);      // 云端文件路径
    void setPassphrase(const QString &pass);      // 加密口令

    // 上传（覆盖） / 下载
    void upload(const QByteArray &plainData);
    void download();

signals:
    void uploadFinished(bool ok, const QString &message);
    void downloadFinished(bool ok, const QByteArray &data, const QString &message);
    void progress(const QString &text);

private:
    QByteArray encrypt(const QByteArray &plain) const;
    QByteArray decrypt(const QByteArray &cipher) const;

    QNetworkAccessManager *m_net = nullptr;
    QString m_baseUrl;
    QString m_user;
    QString m_password;
    QString m_remotePath;
    QString m_passphrase;
};

#endif // SYNCMANAGER_H
