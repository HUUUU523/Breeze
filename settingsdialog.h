#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;

// 设置对话框：主页、默认搜索引擎、隐私（是否记录历史）
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // 读取/写入配置（静态，全局可用）
    static QString homePage();
    static QString searchEngine();
    static bool    recordHistory();
    static void    setHomePage(const QString &url);
    static void    setSearchEngine(const QString &engine);
    static void    setRecordHistory(bool enabled);

    // 代理配置（全局，重启生效）
    static QString proxyType();     // "none" / "http" / "socks5"
    static QString proxyHost();
    static int     proxyPort();
    static QString proxyUser();
    static QString proxyPassword();
    static void    setProxy(const QString &type, const QString &host, int port,
                            const QString &user, const QString &password);
    // 应用当前代理到 QNetworkProxy（启动时调用）
    static void    applyProxy();

    // 下载限速（KB/s，0 = 不限速）
    static int     downloadSpeedLimit();
    static void    setDownloadSpeedLimit(int kbPerSec);

    // 启动行为：0=恢复上次会话，1=打开主页
    static int     startupBehavior();
    static void    setStartupBehavior(int mode);

    // 搜索引擎名 -> 查询 URL 模板（含 %1 占位）
    static QString searchUrlTemplate(const QString &engineName);

private slots:
    void onAccepted();

private:
    void load();

    QLineEdit *m_homeEdit    = nullptr;
    QComboBox *m_engineCombo = nullptr;
    QCheckBox *m_historyCheck = nullptr;
    QComboBox *m_proxyTypeCombo = nullptr;
    QLineEdit *m_proxyHostEdit  = nullptr;
    QSpinBox  *m_proxyPortSpin  = nullptr;
    QSpinBox  *m_speedLimitSpin = nullptr;
    QComboBox *m_startupCombo   = nullptr;
    QLineEdit *m_proxyUserEdit  = nullptr;
    QLineEdit *m_proxyPassEdit  = nullptr;
};

#endif // SETTINGSDIALOG_H
