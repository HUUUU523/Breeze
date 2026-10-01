#include "profiledialog.h"
#include "profilemanager.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QRegularExpression>
#include <QPushButton>
#include <QVBoxLayout>

ProfileDialog::ProfileDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("用户 - Breeze"));
    resize(380, 340);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        QStringLiteral("每个用户拥有独立的书签、历史、设置和会话。\n切换用户后需要重启 Breeze 生效。"), this));

    m_list = new QListWidget(this);
    layout->addWidget(m_list);

    auto *btnRow = new QHBoxLayout;
    auto *newBtn = new QPushButton(QStringLiteral("新建用户"), this);
    m_switchBtn = new QPushButton(QStringLiteral("切换到此用户"), this);
    m_deleteBtn = new QPushButton(QStringLiteral("删除"), this);
    btnRow->addWidget(newBtn);
    btnRow->addWidget(m_switchBtn);
    btnRow->addWidget(m_deleteBtn);
    layout->addLayout(btnRow);

    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    layout->addWidget(closeBtn);

    connect(newBtn, &QPushButton::clicked, this, &ProfileDialog::onNewProfile);
    connect(m_switchBtn, &QPushButton::clicked, this, &ProfileDialog::onSwitch);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ProfileDialog::onDeleteProfile);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    refresh();
}

void ProfileDialog::refresh()
{
    m_list->clear();
    const QString current = ProfileManager::currentProfile();
    const QStringList names = ProfileManager::profiles();
    for (const QString &n : names) {
        auto *item = new QListWidgetItem(n == current
            ? QStringLiteral("%1（当前）").arg(n) : n, m_list);
        item->setData(Qt::UserRole, n);
    }
    // 默认用户不可删除
    m_deleteBtn->setEnabled(m_list->currentItem()
        && m_list->currentItem()->data(Qt::UserRole).toString() != QStringLiteral("默认"));
}

void ProfileDialog::onNewProfile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("新建用户"), QStringLiteral("用户名："),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    if (name.contains(QRegularExpression(QStringLiteral("[\\/:*?\"<>|]")))) {
        QMessageBox::warning(this, QStringLiteral("新建用户"),
                             QStringLiteral("用户名不能包含 \\ / : * ? \" < > | 字符。"));
        return;
    }
    if (!ProfileManager::createProfile(name)) {
        QMessageBox::warning(this, QStringLiteral("新建用户"),
                             QStringLiteral("该用户名已存在。"));
        return;
    }
    refresh();
}

void ProfileDialog::onDeleteProfile()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    const QString name = item->data(Qt::UserRole).toString();
    if (name == QStringLiteral("默认")) {
        QMessageBox::information(this, QStringLiteral("删除用户"),
                                 QStringLiteral("默认用户不可删除。"));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("删除用户"),
            QStringLiteral("确定删除用户「%1」及其所有数据？此操作不可恢复。").arg(name))
        != QMessageBox::Yes)
        return;
    ProfileManager::deleteProfile(name);
    refresh();
}

void ProfileDialog::onSwitch()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    const QString name = item->data(Qt::UserRole).toString();
    if (name == ProfileManager::currentProfile()) {
        QMessageBox::information(this, QStringLiteral("切换用户"),
                                 QStringLiteral("已经是当前用户。"));
        return;
    }
    ProfileManager::setCurrentProfile(name);
    QMessageBox::information(this, QStringLiteral("切换用户"),
        QStringLiteral("已切换到「%1」，请重启 Breeze 生效。").arg(name));
    refresh();
}
