#include "syncmanager.h"

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QUrl>

static const QByteArray kMagic = "BREEZE-SYNC-1";

SyncManager::SyncManager(QObject *parent)
    : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

void SyncManager::setServer(const QString &url, const QString &user, const QString &password)
{
    m_baseUrl = url;
    if (!m_baseUrl.endsWith(QLatin1Char('/')))
        m_baseUrl += QLatin1Char('/');
    m_user = user;
    m_password = password;
}

void SyncManager::setRemotePath(const QString &path)
{
    m_remotePath = path;
}

void SyncManager::setPassphrase(const QString &pass)
{
    m_passphrase = pass;
}

// 由口令派生 32 字节密钥
static QByteArray deriveKey(const QString &passphrase)
{
    return QCryptographicHash::hash(
        passphrase.toUtf8() + QByteArray("breeze-salt"),
        QCryptographicHash::Sha256);
}

// 生成与数据等长的密钥流（用密钥反复哈希扩展）
static QByteArray keyStream(const QByteArray &key, int length)
{
    QByteArray stream;
    QByteArray block = key;
    while (stream.size() < length) {
        block = QCryptographicHash::hash(block, QCryptographicHash::Sha256);
        stream += block;
    }
    return stream.left(length);
}

QByteArray SyncManager::encrypt(const QByteArray &plain) const
{
    if (m_passphrase.isEmpty())
        return plain;   // 无口令则不加密（不建议）

    const QByteArray key = deriveKey(m_passphrase);
    // 随机 IV
    QByteArray iv(16, 0);
    for (int i = 0; i < iv.size(); ++i)
        iv[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));

    const QByteArray stream = keyStream(key + iv, plain.size());
    QByteArray cipher = plain;
    for (int i = 0; i < cipher.size(); ++i)
        cipher[i] = cipher[i] ^ stream[i];

    return kMagic + iv + cipher;
}

QByteArray SyncManager::decrypt(const QByteArray &cipher) const
{
    if (m_passphrase.isEmpty())
        return cipher;

    if (!cipher.startsWith(kMagic))
        return QByteArray();   // 格式不符
    const QByteArray iv = cipher.mid(kMagic.size(), 16);
    const QByteArray body = cipher.mid(kMagic.size() + 16);

    const QByteArray key = deriveKey(m_passphrase);
    const QByteArray stream = keyStream(key + iv, body.size());
    QByteArray plain = body;
    for (int i = 0; i < plain.size(); ++i)
        plain[i] = plain[i] ^ stream[i];
    return plain;
}

void SyncManager::upload(const QByteArray &plainData)
{
    if (m_baseUrl.isEmpty() || m_remotePath.isEmpty()) {
        emit uploadFinished(false, QStringLiteral("未配置 WebDAV 服务器"));
        return;
    }

    const QByteArray payload = encrypt(plainData);
    QNetworkRequest req(QUrl(m_baseUrl + m_remotePath));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/octet-stream"));
    if (!m_user.isEmpty())
        req.setRawHeader("Authorization",
            "Basic " + (m_user + QLatin1Char(':') + m_password).toUtf8().toBase64());

    emit progress(QStringLiteral("正在上传…"));
    QNetworkReply *reply = m_net->put(req, payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit uploadFinished(true, QStringLiteral("上传成功"));
        else
            emit uploadFinished(false, reply->errorString());
    });
}

void SyncManager::testConnection()
{
    if (m_baseUrl.isEmpty()) {
        emit testFinished(false, QStringLiteral("未填写服务器地址"));
        return;
    }

    // 对根目录发 PROPFIND（Depth: 0），WebDAV 服务应返回 207 Multi-Status
    QNetworkRequest req{QUrl(m_baseUrl)};
    req.setRawHeader("Depth", "0");
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml"));
    if (!m_user.isEmpty())
        req.setRawHeader("Authorization",
            "Basic " + (m_user + QLatin1Char(':') + m_password).toUtf8().toBase64());

    emit progress(QStringLiteral("正在测试连接…"));
    const QChar q(34);   // 双引号
    const QByteArray body = QStringLiteral(
        "<?xml version=%11.0%1?>"
        "<d:propfind xmlns:d=%1DAV:%1><d:prop><d:resourcetype/></d:prop></d:propfind>")
        .arg(q).toUtf8();
    QNetworkReply *reply = m_net->sendCustomRequest(req, "PROPFIND", body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int http = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            emit testFinished(false, reply->errorString());
            return;
        }
        if (http == 207 || http == 200) {
            emit testFinished(true, QStringLiteral("连接成功（HTTP %1）").arg(http));
        } else {
            emit testFinished(false,
                QStringLiteral("服务器返回 HTTP %1").arg(http));
        }
    });
}

void SyncManager::download()
{
    if (m_baseUrl.isEmpty() || m_remotePath.isEmpty()) {
        emit downloadFinished(false, QByteArray(), QStringLiteral("未配置 WebDAV 服务器"));
        return;
    }

    QNetworkRequest req(QUrl(m_baseUrl + m_remotePath));
    if (!m_user.isEmpty())
        req.setRawHeader("Authorization",
            "Basic " + (m_user + QLatin1Char(':') + m_password).toUtf8().toBase64());

    emit progress(QStringLiteral("正在下载…"));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit downloadFinished(false, QByteArray(), reply->errorString());
            return;
        }
        const QByteArray plain = decrypt(reply->readAll());
        if (plain.isEmpty()) {
            emit downloadFinished(false, QByteArray(),
                QStringLiteral("解密失败：口令错误或文件损坏"));
            return;
        }
        emit downloadFinished(true, plain, QStringLiteral("下载成功"));
    });
}
