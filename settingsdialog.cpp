#include "settingsdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkProxy>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

static const char *kOrg = "Breeze";
static const char *kApp = "Breeze";

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置 - Breeze"));
    resize(480, 220);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_homeEdit = new QLineEdit(this);
    m_homeEdit->setPlaceholderText(QStringLiteral("https://www.bing.com"));
    auto *homePasteBtn = new QPushButton(QStringLiteral("粘贴"), this);
    auto *homeRow = new QHBoxLayout;
    homeRow->addWidget(m_homeEdit, 1);
    homeRow->addWidget(homePasteBtn);
    form->addRow(QStringLiteral("主页："), homeRow);
    connect(homePasteBtn, &QPushButton::clicked, this, [this]() {
        m_homeEdit->setText(QApplication::clipboard()->text().trimmed());
    });

    m_engineCombo = new QComboBox(this);
    m_engineCombo->addItems({QStringLiteral("Bing"),
                             QStringLiteral("Google"),
                             QStringLiteral("百度"),
                             QStringLiteral("DuckDuckGo")});
    form->addRow(QStringLiteral("默认搜索引擎："), m_engineCombo);

    m_historyCheck = new QCheckBox(QStringLiteral("记录浏览历史"), this);
    form->addRow(QString(), m_historyCheck);

    m_checkUpdateCheck = new QCheckBox(QStringLiteral("启动时检查更新"), this);
    form->addRow(QString(), m_checkUpdateCheck);

    m_startupCombo = new QComboBox(this);
    m_startupCombo->addItem(QStringLiteral("恢复上次会话"), 0);
    m_startupCombo->addItem(QStringLiteral("打开主页"), 1);
    form->addRow(QStringLiteral("启动时："), m_startupCombo);

    m_newTabCombo = new QComboBox(this);
    m_newTabCombo->addItem(QStringLiteral("快速拨号"), 0);
    m_newTabCombo->addItem(QStringLiteral("主页"), 1);
    m_newTabCombo->addItem(QStringLiteral("空白页"), 2);
    form->addRow(QStringLiteral("新标签页："), m_newTabCombo);

    m_minFontSpin = new QSpinBox(this);
    m_minFontSpin->setRange(0, 48);
    m_minFontSpin->setSuffix(QStringLiteral(" px"));
    m_minFontSpin->setSpecialValueText(QStringLiteral("默认"));
    form->addRow(QStringLiteral("网页最小字号："), m_minFontSpin);

    m_defFontSpin = new QSpinBox(this);
    m_defFontSpin->setRange(0, 48);
    m_defFontSpin->setSuffix(QStringLiteral(" px"));
    m_defFontSpin->setSpecialValueText(QStringLiteral("默认"));
    form->addRow(QStringLiteral("网页默认字号："), m_defFontSpin);

    layout->addLayout(form);

    // ---- 代理 ----
    auto *proxyBox = new QGroupBox(QStringLiteral("代理（全局，重启后生效）"), this);
    auto *proxyForm = new QFormLayout(proxyBox);
    m_proxyTypeCombo = new QComboBox(proxyBox);
    m_proxyTypeCombo->addItem(QStringLiteral("不使用代理"), QStringLiteral("none"));
    m_proxyTypeCombo->addItem(QStringLiteral("HTTP"),       QStringLiteral("http"));
    m_proxyTypeCombo->addItem(QStringLiteral("SOCKS5"),     QStringLiteral("socks5"));
    m_proxyHostEdit = new QLineEdit(proxyBox);
    m_proxyHostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
    m_proxyPortSpin = new QSpinBox(proxyBox);
    m_proxyPortSpin->setRange(0, 65535);
    m_proxyUserEdit = new QLineEdit(proxyBox);
    m_proxyPassEdit = new QLineEdit(proxyBox);
    m_proxyPassEdit->setEchoMode(QLineEdit::Password);
    proxyForm->addRow(QStringLiteral("类型："), m_proxyTypeCombo);
    proxyForm->addRow(QStringLiteral("主机："), m_proxyHostEdit);
    proxyForm->addRow(QStringLiteral("端口："), m_proxyPortSpin);
    proxyForm->addRow(QStringLiteral("用户名："), m_proxyUserEdit);
    proxyForm->addRow(QStringLiteral("密码："), m_proxyPassEdit);
    layout->addWidget(proxyBox);

    // ---- 下载限速 ----
    auto *dlBox = new QGroupBox(QStringLiteral("下载"), this);
    auto *dlForm = new QFormLayout(dlBox);
    m_speedLimitSpin = new QSpinBox(dlBox);
    m_speedLimitSpin->setRange(0, 102400);
    m_speedLimitSpin->setSuffix(QStringLiteral(" KB/s"));
    m_speedLimitSpin->setSpecialValueText(QStringLiteral("不限速"));
    dlForm->addRow(QStringLiteral("限速："), m_speedLimitSpin);

    m_downloadDirEdit = new QLineEdit(dlBox);
    m_downloadDirEdit->setPlaceholderText(QStringLiteral("（默认）系统下载目录"));
    auto *browseBtn = new QPushButton(QStringLiteral("浏览…"), dlBox);
    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(m_downloadDirEdit, 1);
    dirRow->addWidget(browseBtn);
    dlForm->addRow(QStringLiteral("下载目录："), dirRow);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择下载目录"), m_downloadDirEdit->text());
        if (!dir.isEmpty())
            m_downloadDirEdit->setText(dir);
    });
    layout->addWidget(dlBox);

    layout->addWidget(new QLabel(QStringLiteral("提示：主页与搜索引擎修改后立即生效。"), this));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *resetBtn = buttons->addButton(QStringLiteral("恢复默认"),
                                        QDialogButtonBox::ResetRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this, QStringLiteral("恢复默认"),
                QStringLiteral("将所有设置恢复为默认值？")) != QMessageBox::Yes)
            return;
        setHomePage(QStringLiteral("https://www.bing.com"));
        setSearchEngine(QStringLiteral("Bing"));
        setRecordHistory(true);
        setDownloadSpeedLimit(0);
        setProxy(QStringLiteral("none"), QString(), 0, QString(), QString());
        setStartupBehavior(0);
        setNewTabBehavior(0);
        load();   // 刷新界面
    });

    load();
}

void SettingsDialog::load()
{
    m_homeEdit->setText(homePage());
    m_engineCombo->setCurrentText(searchEngine());
    m_historyCheck->setChecked(recordHistory());
    m_checkUpdateCheck->setChecked(checkUpdateOnStartup());
    m_startupCombo->setCurrentIndex(startupBehavior() == 1 ? 1 : 0);
    m_newTabCombo->setCurrentIndex(qBound(0, newTabBehavior(), 2));
    m_minFontSpin->setValue(minFontSize());
    m_defFontSpin->setValue(defaultFontSize());

    const QString pt = proxyType();
    const int idx = m_proxyTypeCombo->findData(pt);
    m_proxyTypeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_proxyHostEdit->setText(proxyHost());
    m_proxyPortSpin->setValue(proxyPort());
    m_proxyUserEdit->setText(proxyUser());
    m_proxyPassEdit->setText(proxyPassword());
    m_speedLimitSpin->setValue(downloadSpeedLimit());
    m_downloadDirEdit->setText(downloadDirectory());
}

void SettingsDialog::onAccepted()
{
    setHomePage(m_homeEdit->text().trimmed());
    setSearchEngine(m_engineCombo->currentText());
    setRecordHistory(m_historyCheck->isChecked());
    setCheckUpdateOnStartup(m_checkUpdateCheck->isChecked());
    setStartupBehavior(m_startupCombo->currentData().toInt());
    setNewTabBehavior(m_newTabCombo->currentData().toInt());
    setMinFontSize(m_minFontSpin->value());
    setDefaultFontSize(m_defFontSpin->value());
    setDownloadSpeedLimit(m_speedLimitSpin->value());
    setDownloadDirectory(m_downloadDirEdit->text());
    setProxy(m_proxyTypeCombo->currentData().toString(),
             m_proxyHostEdit->text().trimmed(),
             m_proxyPortSpin->value(),
             m_proxyUserEdit->text(),
             m_proxyPassEdit->text());
    accept();
}

QString SettingsDialog::homePage()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("homePage"),
                   QStringLiteral("https://www.bing.com")).toString();
}

QString SettingsDialog::searchEngine()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("searchEngine"), QStringLiteral("Bing")).toString();
}

bool SettingsDialog::recordHistory()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("recordHistory"), true).toBool();
}

void SettingsDialog::setHomePage(const QString &url)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("homePage"), url.isEmpty()
                   ? QStringLiteral("https://www.bing.com") : url);
}

void SettingsDialog::setSearchEngine(const QString &engine)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("searchEngine"), engine);
}

void SettingsDialog::setRecordHistory(bool enabled)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("recordHistory"), enabled);
}

QString SettingsDialog::searchUrlTemplate(const QString &engineName)
{
    if (engineName == QStringLiteral("Google"))
        return QStringLiteral("https://www.google.com/search?q=%1");
    if (engineName == QStringLiteral("百度"))
        return QStringLiteral("https://www.baidu.com/s?wd=%1");
    if (engineName == QStringLiteral("DuckDuckGo"))
        return QStringLiteral("https://duckduckgo.com/?q=%1");
    return QStringLiteral("https://www.bing.com/search?q=%1");
}

// ===================== 代理 =====================

QString SettingsDialog::proxyType()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("proxy/type"), QStringLiteral("none")).toString();
}

QString SettingsDialog::proxyHost()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("proxy/host")).toString();
}

int SettingsDialog::proxyPort()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("proxy/port"), 0).toInt();
}

QString SettingsDialog::proxyUser()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("proxy/user")).toString();
}

QString SettingsDialog::proxyPassword()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("proxy/password")).toString();
}

void SettingsDialog::setProxy(const QString &type, const QString &host, int port,
                              const QString &user, const QString &password)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("proxy/type"), type);
    s.setValue(QStringLiteral("proxy/host"), host);
    s.setValue(QStringLiteral("proxy/port"), port);
    s.setValue(QStringLiteral("proxy/user"), user);
    s.setValue(QStringLiteral("proxy/password"), password);
}

int SettingsDialog::downloadSpeedLimit()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("download/speedLimitKB"), 0).toInt();
}

void SettingsDialog::setDownloadSpeedLimit(int kbPerSec)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("download/speedLimitKB"), qMax(0, kbPerSec));
}

QString SettingsDialog::downloadDirectory()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("download/directory")).toString();
}

void SettingsDialog::setDownloadDirectory(const QString &dir)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("download/directory"), dir.trimmed());
}

int SettingsDialog::minFontSize()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("web/minFontSize"), 0).toInt();
}

void SettingsDialog::setMinFontSize(int px)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("web/minFontSize"), qMax(0, px));
}

int SettingsDialog::defaultFontSize()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("web/defaultFontSize"), 0).toInt();
}

void SettingsDialog::setDefaultFontSize(int px)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("web/defaultFontSize"), qMax(0, px));
}

bool SettingsDialog::checkUpdateOnStartup()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("update/checkOnStartup"), false).toBool();
}

void SettingsDialog::setCheckUpdateOnStartup(bool enabled)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("update/checkOnStartup"), enabled);
}

int SettingsDialog::startupBehavior()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("startup/behavior"), 0).toInt();
}

void SettingsDialog::setStartupBehavior(int mode)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("startup/behavior"), mode);
}

int SettingsDialog::newTabBehavior()
{
    QSettings s(kOrg, kApp);
    return s.value(QStringLiteral("newtab/behavior"), 0).toInt();
}

void SettingsDialog::setNewTabBehavior(int mode)
{
    QSettings s(kOrg, kApp);
    s.setValue(QStringLiteral("newtab/behavior"), mode);
}

void SettingsDialog::applyProxy()
{
    const QString type = proxyType();
    if (type == QStringLiteral("none") || type.isEmpty()) {
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        return;
    }

    QNetworkProxy proxy;
    proxy.setType(type == QStringLiteral("socks5")
                      ? QNetworkProxy::Socks5Proxy
                      : QNetworkProxy::HttpProxy);
    proxy.setHostName(proxyHost());
    proxy.setPort(static_cast<quint16>(proxyPort()));
    if (!proxyUser().isEmpty()) {
        proxy.setUser(proxyUser());
        proxy.setPassword(proxyPassword());
    }
    QNetworkProxy::setApplicationProxy(proxy);
}
