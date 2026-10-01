#ifndef ACCOUNTDIALOG_H
#define ACCOUNTDIALOG_H

#include <QDialog>

class QLineEdit;
class QLabel;
class QPushButton;
class QTabWidget;
class AccountManager;

// 账号对话框：注册 / 登录 / 显示当前登录状态
class AccountDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AccountDialog(AccountManager *account, QWidget *parent = nullptr);

private slots:
    void onSignIn();
    void onSignUp();
    void onSignOut();
    void onForgotPassword();
    void refreshUi();

private:
    AccountManager *m_account = nullptr;

    QTabWidget  *m_tabs = nullptr;
    QLineEdit   *m_loginEmail = nullptr;
    QLineEdit   *m_loginPass = nullptr;
    QLineEdit   *m_regEmail = nullptr;
    QLineEdit   *m_regPass = nullptr;
    QLineEdit   *m_regPass2 = nullptr;
    QLabel      *m_status = nullptr;
    QPushButton *m_signOutBtn = nullptr;
};

#endif // ACCOUNTDIALOG_H
