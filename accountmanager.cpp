#include "accountmanager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

// Supabase 项目配置
namespace {
const char *kSupabaseUrl = "https://swjfbdlaxxnrodfqzhgm.supabase.co";
const char *kSupabaseKey = "sb_publishable_yG8cRQA8b93IQPhf6FHHaw_YqKjyNf1";
}

AccountManager::AccountManager(QObject *parent)
    : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

void AccountManager::loadSession()
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    m_accessToken  = s.value(QStringLiteral("account/accessToken")).toString();
    m_refreshToken = s.value(QStringLiteral("account/refreshToken")).toString();
    m_email        = s.value(QStringLiteral("account/email")).toString();
    m_userId       = s.value(QStringLiteral("account/userId")).toString();
}

void AccountManager::clearSession()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_email.clear();
    m_userId.clear();
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.remove(QStringLiteral("account"));
}

void AccountManager::signUp(const QString &email, const QString &password)
{
    QNetworkRequest req{QUrl(QString::fromLatin1(kSupabaseUrl)
                             + QStringLiteral("/auth/v1/signup"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("apikey", kSupabaseKey);

    QJsonObject body;
    body.insert(QStringLiteral("email"), email);
    body.insert(QStringLiteral("password"), password);

    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleAuthReply(reply, true);
    });
}

void AccountManager::signIn(const QString &email, const QString &password)
{
    QNetworkRequest req{QUrl(QString::fromLatin1(kSupabaseUrl)
                             + QStringLiteral("/auth/v1/token?grant_type=password"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("apikey", kSupabaseKey);

    QJsonObject body;
    body.insert(QStringLiteral("email"), email);
    body.insert(QStringLiteral("password"), password);

    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleAuthReply(reply, false);
    });
}

void AccountManager::signOut()
{
    clearSession();
    emit signOutFinished();
}

void AccountManager::resetPassword(const QString &email)
{
    QNetworkRequest req{QUrl(QString::fromLatin1(kSupabaseUrl)
                             + QStringLiteral("/auth/v1/recover"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("apikey", kSupabaseKey);

    QJsonObject body;
    body.insert(QStringLiteral("email"), email);

    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            QString msg = o.value(QStringLiteral("msg")).toString();
            if (msg.isEmpty())
                msg = reply->errorString();
            emit resetPasswordFinished(false, msg);
        } else {
            emit resetPasswordFinished(true,
                QStringLiteral("重置密码邮件已发送，请查收邮箱。"));
        }
    });
}

void AccountManager::handleAuthReply(QNetworkReply *reply, bool isSignUp)
{
    reply->deleteLater();
    const QByteArray data = reply->readAll();
    const QJsonObject obj = QJsonDocument::fromJson(data).object();

    if (reply->error() != QNetworkReply::NoError) {
        // Supabase 错误信息在 msg / error_description 字段
        QString msg = obj.value(QStringLiteral("msg")).toString();
        if (msg.isEmpty())
            msg = obj.value(QStringLiteral("error_description")).toString();
        if (msg.isEmpty())
            msg = obj.value(QStringLiteral("message")).toString();
        if (msg.isEmpty())
            msg = reply->errorString();
        if (isSignUp)
            emit signUpFinished(false, msg);
        else
            emit signInFinished(false, msg);
        return;
    }

    // 注册可能返回 user 但无 session（需要邮箱验证）
    if (isSignUp && !obj.contains(QStringLiteral("access_token"))) {
        emit signUpFinished(true,
            QStringLiteral("注册成功，请查收邮箱完成验证后再登录。"));
        return;
    }

    m_accessToken  = obj.value(QStringLiteral("access_token")).toString();
    m_refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
    const QJsonObject user = obj.value(QStringLiteral("user")).toObject();
    m_email  = user.value(QStringLiteral("email")).toString();
    m_userId = user.value(QStringLiteral("id")).toString();

    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("account/accessToken"), m_accessToken);
    s.setValue(QStringLiteral("account/refreshToken"), m_refreshToken);
    s.setValue(QStringLiteral("account/email"), m_email);
    s.setValue(QStringLiteral("account/userId"), m_userId);

    if (isSignUp)
        emit signUpFinished(true, QStringLiteral("注册并登录成功：") + m_email);
    else
        emit signInFinished(true, QStringLiteral("登录成功：") + m_email);
}
