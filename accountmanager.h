#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;

// Supabase 账号系统：封装注册/登录/登出（REST API）
class AccountManager : public QObject
{
    Q_OBJECT

public:
    explicit AccountManager(QObject *parent = nullptr);

    // 当前登录状态
    bool isLoggedIn() const { return !m_accessToken.isEmpty(); }
    QString email() const { return m_email; }
    QString userId() const { return m_userId; }

    // 从 QSettings 恢复/保存会话
    void loadSession();
    void clearSession();

    // 操作
    void signUp(const QString &email, const QString &password);
    void signIn(const QString &email, const QString &password);
    void signOut();

signals:
    void signUpFinished(bool ok, const QString &message);
    void signInFinished(bool ok, const QString &message);
    void signOutFinished();

private:
    void handleAuthReply(class QNetworkReply *reply, bool isSignUp);

    QNetworkAccessManager *m_net = nullptr;
    QString m_accessToken;
    QString m_refreshToken;
    QString m_email;
    QString m_userId;
};

#endif // ACCOUNTMANAGER_H
