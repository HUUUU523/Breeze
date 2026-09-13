#include "userscriptmanager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

// ---- 匹配规则：把 *://*.host/path* 形式的通配转成正则 ----
static bool matchPattern(const QString &pattern, const QUrl &url)
{
    QString regex = QRegularExpression::escape(pattern);
    regex.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
    // QRegularExpression::escape 把 * 转成 *，上面还原为 .*
    QRegularExpression re(QStringLiteral("^") + regex + QStringLiteral("$"),
                          QRegularExpression::CaseInsensitiveOption);
    return re.match(url.toString()).hasMatch();
}

bool UserScriptManager::matchesUrl(const UserScript &script, const QUrl &url)
{
    if (script.match.trimmed().isEmpty())
        return false;
    // 支持多个 match，用换行或逗号分隔
    const QStringList patterns = script.match.split(
        QRegularExpression(QStringLiteral("[,\n]")), Qt::SkipEmptyParts);
    for (const QString &p : patterns) {
        if (matchPattern(p.trimmed(), url))
            return true;
    }
    return false;
}

void UserScriptManager::parseMetadata(UserScript &script)
{
    // 只解析头部 ==UserScript== ... ==/UserScript== 块
    const QString head = script.code.left(4000);
    const int begin = head.indexOf(QStringLiteral("==UserScript=="));
    if (begin < 0)
        return;
    const int end = head.indexOf(QStringLiteral("==/UserScript=="), begin);
    if (end < 0)
        return;
    const QString block = head.mid(begin, end - begin);

    QStringList matches;
    for (const QString &rawLine : block.split(QLatin1Char('\n'))) {
        QString line = rawLine.trimmed();
        if (!line.startsWith(QLatin1Char('@')))
            continue;
        const int sp = line.indexOf(QRegularExpression(QStringLiteral("\\s")));
        if (sp < 0)
            continue;
        const QString key = line.mid(1, sp - 1).trimmed().toLower();
        const QString val = line.mid(sp + 1).trimmed();
        if (val.isEmpty())
            continue;

        if (key == QStringLiteral("name"))
            script.name = val;
        else if (key == QStringLiteral("description"))
            script.description = val;
        else if (key == QStringLiteral("match") || key == QStringLiteral("include"))
            matches << val;
        else if (key == QStringLiteral("run-at"))
            script.runAt = val;
        else if (key == QStringLiteral("grant"))
            script.grants << val;
        else if (key == QStringLiteral("require"))
            script.requires << val;
    }

    // @match 优先覆盖手动填写的 match（若解析到）
    if (!matches.isEmpty())
        script.match = matches.join(QLatin1Char('\n'));
}

static QString scriptsFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/userscripts.json");
}

QList<UserScript> UserScriptManager::loadScripts()
{
    QList<UserScript> list;
    QFile f(scriptsFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return list;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        UserScript s;
        s.name    = o.value(QStringLiteral("name")).toString();
        s.match   = o.value(QStringLiteral("match")).toString();
        s.code    = o.value(QStringLiteral("code")).toString();
        s.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        s.isCss   = o.value(QStringLiteral("isCss")).toBool(false);
        if (!s.name.isEmpty())
            list.append(s);
    }
    return list;
}

void UserScriptManager::saveScripts(const QList<UserScript> &scripts)
{
    QJsonArray arr;
    for (const UserScript &s : scripts) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), s.name);
        o.insert(QStringLiteral("match"), s.match);
        o.insert(QStringLiteral("code"), s.code);
        o.insert(QStringLiteral("enabled"), s.enabled);
        o.insert(QStringLiteral("isCss"), s.isCss);
        arr.append(o);
    }
    QFile f(scriptsFilePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
        f.close();
    }
}

UserScriptManager::UserScriptManager(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("用户脚本管理 - Breeze"));
    resize(720, 460);

    auto *layout = new QVBoxLayout(this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("启用"), QStringLiteral("名称"), QStringLiteral("匹配规则")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    auto *btns = new QHBoxLayout;
    auto *addB = new QPushButton(QStringLiteral("新建"), this);
    auto *editB = new QPushButton(QStringLiteral("编辑"), this);
    auto *delB = new QPushButton(QStringLiteral("删除"), this);
    auto *closeB = new QPushButton(QStringLiteral("关闭"), this);
    btns->addWidget(addB);
    btns->addWidget(editB);
    btns->addWidget(delB);
    btns->addStretch();
    btns->addWidget(closeB);
    layout->addLayout(btns);

    connect(addB, &QPushButton::clicked, this, &UserScriptManager::onAdd);
    connect(editB, &QPushButton::clicked, this, &UserScriptManager::onEdit);
    connect(delB, &QPushButton::clicked, this, &UserScriptManager::onRemove);
    connect(closeB, &QPushButton::clicked, this, &QDialog::accept);

    m_scripts = loadScripts();
    reload();
}

int UserScriptManager::currentRow() const
{
    return m_table->currentRow();
}

void UserScriptManager::reload()
{
    m_table->setRowCount(0);
    for (const UserScript &s : m_scripts) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *chk = new QCheckBox(m_table);
        chk->setChecked(s.enabled);
        m_table->setCellWidget(row, 0, chk);
        connect(chk, &QCheckBox::toggled, this, [this, row](bool on) {
            if (row >= 0 && row < m_scripts.size()) {
                m_scripts[row].enabled = on;
                saveScripts(m_scripts);
            }
        });

        m_table->setItem(row, 1, new QTableWidgetItem(s.name));
        m_table->setItem(row, 2, new QTableWidgetItem(s.match));
    }
}

void UserScriptManager::onAdd()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("新建用户脚本"));
    dlg.resize(560, 420);
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout;
    auto *nameE = new QLineEdit(&dlg);
    auto *matchE = new QLineEdit(&dlg);
    matchE->setPlaceholderText(QStringLiteral("*://*.example.com/*"));
    auto *typeCombo = new QComboBox(&dlg);
    typeCombo->addItem(QStringLiteral("JavaScript"), false);
    typeCombo->addItem(QStringLiteral("CSS"), true);
    auto *codeE = new QTextEdit(&dlg);
    codeE->setPlaceholderText(QStringLiteral("// JavaScript 或 CSS"));
    form->addRow(QStringLiteral("名称："), nameE);
    form->addRow(QStringLiteral("匹配："), matchE);
    form->addRow(QStringLiteral("类型："), typeCombo);
    lay->addLayout(form);
    lay->addWidget(new QLabel(QStringLiteral("代码："), &dlg));
    lay->addWidget(codeE);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (nameE->text().trimmed().isEmpty() || codeE->toPlainText().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Breeze"),
                             QStringLiteral("名称和代码不能为空。"));
        return;
    }
    UserScript s;
    s.name = nameE->text().trimmed();
    s.match = matchE->text().trimmed();
    s.code = codeE->toPlainText();
    s.enabled = true;
    s.isCss = typeCombo->currentData().toBool();
    m_scripts.append(s);
    saveScripts(m_scripts);
    reload();
}

void UserScriptManager::onEdit()
{
    const int row = currentRow();
    if (row < 0 || row >= m_scripts.size())
        return;
    UserScript &s = m_scripts[row];

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("编辑用户脚本"));
    dlg.resize(560, 420);
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout;
    auto *nameE = new QLineEdit(s.name, &dlg);
    auto *matchE = new QLineEdit(s.match, &dlg);
    auto *codeE = new QTextEdit(&dlg);
    codeE->setPlainText(s.code);
    form->addRow(QStringLiteral("名称："), nameE);
    form->addRow(QStringLiteral("匹配："), matchE);
    lay->addLayout(form);
    lay->addWidget(new QLabel(QStringLiteral("代码："), &dlg));
    lay->addWidget(codeE);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;
    s.name = nameE->text().trimmed();
    s.match = matchE->text().trimmed();
    s.code = codeE->toPlainText();
    saveScripts(m_scripts);
    reload();
}

void UserScriptManager::onRemove()
{
    const int row = currentRow();
    if (row < 0 || row >= m_scripts.size())
        return;
    if (QMessageBox::question(this, QStringLiteral("Breeze"),
                              QStringLiteral("删除选中的用户脚本？"))
        != QMessageBox::Yes)
        return;
    m_scripts.removeAt(row);
    saveScripts(m_scripts);
    reload();
}
