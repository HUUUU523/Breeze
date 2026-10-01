#include "readinglistmanager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

ReadingListDialog::ReadingListDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("稍后读 - Breeze"));
    resize(560, 420);

    auto *layout = new QVBoxLayout(this);
    m_stat = new QLabel(this);
    layout->addWidget(m_stat);

    m_list = new QListWidget(this);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list);

    auto *row = new QHBoxLayout;
    auto *openBtn = new QPushButton(QStringLiteral("打开"), this);
    auto *readBtn = new QPushButton(QStringLiteral("标记已读/未读"), this);
    auto *delBtn = new QPushButton(QStringLiteral("删除"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    row->addWidget(openBtn);
    row->addWidget(readBtn);
    row->addWidget(delBtn);
    row->addStretch();
    row->addWidget(closeBtn);
    layout->addLayout(row);

    connect(openBtn, &QPushButton::clicked, this, &ReadingListDialog::onItemActivated);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &ReadingListDialog::onItemActivated);
    connect(readBtn, &QPushButton::clicked, this, &ReadingListDialog::onToggleRead);
    connect(delBtn, &QPushButton::clicked, this, &ReadingListDialog::onRemove);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &ReadingListDialog::onContextMenu);
}

void ReadingListDialog::setItems(const QList<ReadingItem> &items)
{
    m_items = items;
    rebuild();
}

void ReadingListDialog::rebuild()
{
    m_list->clear();
    int unread = 0;
    for (const ReadingItem &it : m_items) {
        if (!it.read)
            ++unread;
        const QString prefix = it.read ? QStringLiteral("✓ ") : QStringLiteral("● ");
        auto *item = new QListWidgetItem(prefix + it.title, m_list);
        item->setData(Qt::UserRole, it.url.toString());
        if (it.read)
            item->setForeground(QColor(150, 150, 150));
        item->setToolTip(it.url.toString());
    }
    m_stat->setText(QStringLiteral("共 %1 条，未读 %2 条").arg(m_items.size()).arg(unread));
}

void ReadingListDialog::onItemActivated()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    emit openUrlRequested(QUrl(item->data(Qt::UserRole).toString()));
}

void ReadingListDialog::onToggleRead()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    emit toggleReadRequested(QUrl(item->data(Qt::UserRole).toString()));
}

void ReadingListDialog::onRemove()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    emit removeRequested(QUrl(item->data(Qt::UserRole).toString()));
}

void ReadingListDialog::onContextMenu(const QPoint &pos)
{
    auto *item = m_list->itemAt(pos);
    if (!item)
        return;
    m_list->setCurrentItem(item);
    QMenu menu(this);
    QAction *aOpen = menu.addAction(QStringLiteral("打开"));
    QAction *aRead = menu.addAction(QStringLiteral("切换已读"));
    menu.addSeparator();
    QAction *aDel = menu.addAction(QStringLiteral("删除"));
    QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == aOpen) onItemActivated();
    else if (chosen == aRead) onToggleRead();
    else if (chosen == aDel) onRemove();
}
