#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
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
}

void SettingsDialog::onAccepted()
{
    setHomePage(m_homeEdit->text().trimmed());
    setSearchEngine(m_engineCombo->currentText());
    setRecordHistory(m_historyCheck->isChecked());
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
