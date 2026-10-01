#ifndef SYNCDIALOG_H
#define SYNCDIALOG_H

#include <QDialog>

class QLineEdit;
class QComboBox;
class QPushButton;
class BrowserWindow;
class AccountManager;

// 云同步配置与操作对话框
class SyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncDialog(BrowserWindow *browser);

    // 设置账号管理器：登录后按账号隔离云端文件名
    void setAccount(AccountManager *account);

private slots:
    void onUpload();
    void onDownload();
    void onQuickLogin();       // 快速登录：按服务商拼 WebDAV 地址
    void onProviderChanged();  // 切换服务商时调整输入框提示

private:
    void loadConfig();
    void saveConfig();

    BrowserWindow *m_browser = nullptr;
    AccountManager *m_account = nullptr;

    // 快速登录
    QComboBox   *m_providerCombo = nullptr;
    QLineEdit   *m_quickAccount  = nullptr;   // 邮箱 / 用户名
    QLineEdit   *m_quickPassword = nullptr;   // 应用密码
    QPushButton *m_loginBtn      = nullptr;

    // 手动配置（登录成功后自动回填）
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passEdit = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLineEdit *m_passphraseEdit = nullptr;
};

#endif // SYNCDIALOG_H
