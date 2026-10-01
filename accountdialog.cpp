#include "accountdialog.h"
#include "accountmanager.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

AccountDialog::AccountDialog(AccountManager *account, QWidget *parent)
    : QDialog(parent), m_account(account)
{
    setWindowTitle(QStringLiteral("账号 - Breeze"));
    resize(400, 320);

    auto *layout = new QVBoxLayout(this);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    m_tabs = new QTabWidget(this);

    // ---- 登录页 ----
    auto *loginPage = new QWidget(this);
    auto *loginForm = new QFormLayout(loginPage);
    m_loginEmail = new QLineEdit(loginPage);
    m_loginEmail->setPlaceholderText(QStringLiteral("邮箱"));
    m_loginPass = new QLineEdit(loginPage);
    m_loginPass->setEchoMode(QLineEdit::Password);
    auto *loginBtn = new QPushButton(QStringLiteral("登录"), loginPage);
    loginForm->addRow(QStringLiteral("邮箱："), m_loginEmail);
    loginForm->addRow(QStringLiteral("密码："), m_loginPass);
    loginForm->addRow(QString(), loginBtn);
    m_tabs->addTab(loginPage, QStringLiteral("登录"));

    // ---- 注册页 ----
    auto *regPage = new QWidget(this);
    auto *regForm = new QFormLayout(regPage);
    m_regEmail = new QLineEdit(regPage);
    m_regEmail->setPlaceholderText(QStringLiteral("邮箱"));
    m_regPass = new QLineEdit(regPage);
    m_regPass->setEchoMode(QLineEdit::Password);
    m_regPass2 = new QLineEdit(regPage);
    m_regPass2->setEchoMode(QLineEdit::Password);
    auto *regBtn = new QPushButton(QStringLiteral("注册"), regPage);
    regForm->addRow(QStringLiteral("邮箱："), m_regEmail);
    regForm->addRow(QStringLiteral("密码："), m_regPass);
    regForm->addRow(QStringLiteral("确认密码："), m_regPass2);
    regForm->addRow(QString(), regBtn);
    m_tabs->addTab(regPage, QStringLiteral("注册"));

    layout->addWidget(m_tabs);

    m_signOutBtn = new QPushButton(QStringLiteral("退出登录"), this);
    layout->addWidget(m_signOutBtn);

    connect(loginBtn, &QPushButton::clicked, this, &AccountDialog::onSignIn);
    connect(regBtn, &QPushButton::clicked, this, &AccountDialog::onSignUp);
    connect(m_signOutBtn, &QPushButton::clicked, this, &AccountDialog::onSignOut);
    connect(m_account, &AccountManager::signInFinished, this,
            [this](bool ok, const QString &msg) {
                if (ok) { QMessageBox::information(this, QStringLiteral("账号"), msg); refreshUi(); }
                else    { QMessageBox::warning(this, QStringLiteral("账号"), msg); }
            });
    connect(m_account, &AccountManager::signUpFinished, this,
            [this](bool ok, const QString &msg) {
                if (ok) { QMessageBox::information(this, QStringLiteral("账号"), msg); refreshUi(); }
                else    { QMessageBox::warning(this, QStringLiteral("账号"), msg); }
            });
    connect(m_account, &AccountManager::signOutFinished, this,
            [this]() { refreshUi(); });

    refreshUi();
}

void AccountDialog::onSignIn()
{
    const QString email = m_loginEmail->text().trimmed();
    const QString pass = m_loginPass->text();
    if (email.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("账号"),
                             QStringLiteral("请填写邮箱和密码。"));
        return;
    }
    m_account->signIn(email, pass);
}

void AccountDialog::onSignUp()
{
    const QString email = m_regEmail->text().trimmed();
    const QString pass = m_regPass->text();
    if (email.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("账号"),
                             QStringLiteral("请填写邮箱和密码。"));
        return;
    }
    if (pass != m_regPass2->text()) {
        QMessageBox::warning(this, QStringLiteral("账号"),
                             QStringLiteral("两次输入的密码不一致。"));
        return;
    }
    if (pass.length() < 6) {
        QMessageBox::warning(this, QStringLiteral("账号"),
                             QStringLiteral("密码至少 6 位。"));
        return;
    }
    m_account->signUp(email, pass);
}

void AccountDialog::onSignOut()
{
    m_account->signOut();
}

void AccountDialog::refreshUi()
{
    const bool logged = m_account->isLoggedIn();
    if (logged) {
        m_status->setText(QStringLiteral("已登录：<b>%1</b>").arg(m_account->email()));
        m_tabs->setEnabled(false);
        m_signOutBtn->setEnabled(true);
    } else {
        m_status->setText(QStringLiteral("未登录"));
        m_tabs->setEnabled(true);
        m_signOutBtn->setEnabled(false);
    }
}
