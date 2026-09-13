#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
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
    form->addRow(QStringLiteral("主页："), m_homeEdit);

    m_engineCombo = new QComboBox(this);
    m_engineCombo->addItems({QStringLiteral("Bing"),
                             QStringLiteral("Google"),
                             QStringLiteral("百度"),
                             QStringLiteral("DuckDuckGo")});
    form->addRow(QStringLiteral("默认搜索引擎："), m_engineCombo);

    m_historyCheck = new QCheckBox(QStringLiteral("记录浏览历史"), this);
    form->addRow(QString(), m_historyCheck);

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
    layout->addWidget(dlBox);

    layout->addWidget(new QLabel(QStringLiteral("提示：主页与搜索引擎修改后立即生效。"), this));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    load();
}

void SettingsDialog::load()
{
    m_homeEdit->setText(homePage());
    m_engineCombo->setCurrentText(searchEngine());
    m_historyCheck->setChecked(recordHistory());

    const QString pt = proxyType();
    const int idx = m_proxyTypeCombo->findData(pt);
    m_proxyTypeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_proxyHostEdit->setText(proxyHost());
    m_proxyPortSpin->setValue(proxyPort());
    m_proxyUserEdit->setText(proxyUser());
    m_proxyPassEdit->setText(proxyPassword());
    m_speedLimitSpin->setValue(downloadSpeedLimit());
}

void SettingsDialog::onAccepted()
{
    setHomePage(m_homeEdit->text().trimmed());
    setSearchEngine(m_engineCombo->currentText());
    setRecordHistory(m_historyCheck->isChecked());
    setDownloadSpeedLimit(m_speedLimitSpin->value());
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
