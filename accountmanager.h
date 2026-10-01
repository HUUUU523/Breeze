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
    QString nickname() const { return m_nickname; }
    QString avatarUrl() const { return m_avatarUrl; }
    QString displayName() const;   // 昵称优先，回退到 email 前缀

    // 从 QSettings 恢复/保存会话
    void loadSession();
    void clearSession();

    // 操作
    void signUp(const QString &email, const QString &password);
    void signIn(const QString &email, const QString &password);
    void signOut();
    void resetPassword(const QString &email);   // 发送重置密码邮件
    void updateNickname(const QString &nickname);  // 修改昵称

signals:
    void signUpFinished(bool ok, const QString &message);
    void signInFinished(bool ok, const QString &message);
    void signOutFinished();
    void resetPasswordFinished(bool ok, const QString &message);
    void nicknameUpdated(bool ok, const QString &message);

private:
    void handleAuthReply(class QNetworkReply *reply, bool isSignUp);

    QNetworkAccessManager *m_net = nullptr;
    QString m_accessToken;
    QString m_refreshToken;
    QString m_email;
    QString m_userId;
    QString m_nickname;
    QString m_avatarUrl;
};

#endif // ACCOUNTMANAGER_H
