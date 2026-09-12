#include "bookmarkmanager.h"
#include "browserwindow.h"   // for struct Bookmark

#include <algorithm>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

BookmarkManager::BookmarkManager(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("书签管理 - Breeze"));
    resize(720, 520);

    auto *layout = new QVBoxLayout(this);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({QStringLiteral("名称"), QStringLiteral("地址")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &BookmarkManager::onItemDoubleClicked);
    connect(m_tree->model(), &QAbstractItemModel::rowsMoved,
            this, &BookmarkManager::onItemMoved);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                QTreeWidgetItem *item = m_tree->itemAt(pos);
                QMenu menu;
                if (item) {
                    menu.addAction(QStringLiteral("重命名"), this, &BookmarkManager::onRenameSelected);
                    menu.addAction(QStringLiteral("删除"), this, &BookmarkManager::onRemoveSelected);
                }
                menu.addAction(QStringLiteral("新建分组..."), this, &BookmarkManager::onAddGroup);
                menu.exec(m_tree->mapToGlobal(pos));
            });

    layout->addWidget(m_tree);

    auto *btns = new QHBoxLayout;
    auto *addB = new QPushButton(QStringLiteral("新建书签"), this);
    auto *addG = new QPushButton(QStringLiteral("新建分组"), this);
    auto *delB = new QPushButton(QStringLiteral("删除"), this);
    auto *closeB = new QPushButton(QStringLiteral("关闭"), this);
    btns->addWidget(addB);
    btns->addWidget(addG);
    btns->addWidget(delB);
    btns->addStretch();
    btns->addWidget(closeB);
    layout->addLayout(btns);

    connect(addB, &QPushButton::clicked, this, &BookmarkManager::onAddBookmark);
    connect(addG, &QPushButton::clicked, this, &BookmarkManager::onAddGroup);
    connect(delB, &QPushButton::clicked, this, &BookmarkManager::onRemoveSelected);
    connect(closeB, &QPushButton::clicked, this, &QDialog::accept);
}

void BookmarkManager::setBookmarks(QList<Bookmark> *bookmarks)
{
    m_bookmarks = bookmarks;
    rebuild();
}

QStringList BookmarkManager::allGroups() const
{
    QStringList groups;
    if (!m_bookmarks)
        return groups;
    for (const Bookmark &b : *m_bookmarks) {
        if (!b.group.isEmpty() && !groups.contains(b.group))
            groups << b.group;
    }
    return groups;
}

void BookmarkManager::rebuild()
{
    m_tree->clear();
    if (!m_bookmarks)
        return;

    // 未分组作为顶层项直接列出
    for (int i = 0; i < m_bookmarks->size(); ++i) {
        const Bookmark &b = m_bookmarks->at(i);
        if (!b.group.isEmpty())
            continue;
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, b.title.isEmpty() ? b.url.host() : b.title);
        item->setText(1, b.url.toString());
        item->setData(0, Qt::UserRole, i);          // 索引
    }

    // 分组作为顶层容器
    for (const QString &g : allGroups()) {
        auto *groupItem = new QTreeWidgetItem(m_tree);
        groupItem->setText(0, QStringLiteral("📁 ") + g);
        groupItem->setData(0, Qt::UserRole, -1);    // 分组标记
        groupItem->setData(0, Qt::UserRole + 1, g);
        groupItem->setFlags(groupItem->flags() | Qt::ItemIsDropEnabled);

        for (int i = 0; i < m_bookmarks->size(); ++i) {
            const Bookmark &b = m_bookmarks->at(i);
            if (b.group != g)
                continue;
            auto *item = new QTreeWidgetItem(groupItem);
            item->setText(0, b.title.isEmpty() ? b.url.host() : b.title);
            item->setText(1, b.url.toString());
            item->setData(0, Qt::UserRole, i);
            item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);  // 书签不能接收拖放
        }
        groupItem->setExpanded(true);
    }
}

void BookmarkManager::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item || !m_bookmarks)
        return;
    const int idx = item->data(0, Qt::UserRole).toInt();
    if (idx < 0 || idx >= m_bookmarks->size())
        return;
    // 编辑书名
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("编辑书签"), QStringLiteral("名称："),
        QLineEdit::Normal, item->text(0), &ok);
    if (ok && !name.isEmpty()) {
        (*m_bookmarks)[idx].title = name;
        item->setText(0, name);
        emit changed();
    }
}

void BookmarkManager::onAddBookmark()
{
    if (!m_bookmarks)
        return;
    bool ok = false;
    const QString title = QInputDialog::getText(
        this, QStringLiteral("新建书签"), QStringLiteral("名称："),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || title.isEmpty())
        return;
    const QString url = QInputDialog::getText(
        this, QStringLiteral("新建书签"), QStringLiteral("地址："),
        QLineEdit::Normal, QStringLiteral("https://"), &ok);
    if (!ok || url.isEmpty())
        return;

    Bookmark b;
    b.title = title;
    b.url = QUrl(url);
    if (!b.url.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Breeze"),
                             QStringLiteral("地址无效。"));
        return;
    }

    // 若当前选中分组，则加入该分组
    QTreeWidgetItem *cur = m_tree->currentItem();
    if (cur) {
        if (cur->data(0, Qt::UserRole).toInt() == -1)
            b.group = cur->data(0, Qt::UserRole + 1).toString();
        else if (cur->parent())
            b.group = cur->parent()->data(0, Qt::UserRole + 1).toString();
    }

    m_bookmarks->append(b);
    rebuild();
    emit changed();
}

void BookmarkManager::onAddGroup()
{
    if (!m_bookmarks)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("新建分组"), QStringLiteral("分组名称："),
        QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    // 分组本身不存数据，等第一个书签加入时才出现。
    // 这里用一个临时空分组项提示用户后续拖入。
    auto *groupItem = new QTreeWidgetItem(m_tree);
    groupItem->setText(0, QStringLiteral("📁 ") + name);
    groupItem->setData(0, Qt::UserRole, -1);
    groupItem->setData(0, Qt::UserRole + 1, name);
    groupItem->setFlags(groupItem->flags() | Qt::ItemIsDropEnabled);
    m_tree->addTopLevelItem(groupItem);
}

void BookmarkManager::onRemoveSelected()
{
    if (!m_bookmarks)
        return;
    const auto items = m_tree->selectedItems();
    if (items.isEmpty())
        return;
    if (QMessageBox::question(this, QStringLiteral("Breeze"),
                              QStringLiteral("删除选中的书签/分组？"))
        != QMessageBox::Yes)
        return;

    // 收集要删除的索引
    QList<int> toRemove;
    for (QTreeWidgetItem *item : items) {
        const int idx = item->data(0, Qt::UserRole).toInt();
        if (idx >= 0)
            toRemove << idx;
        // 分组项：连同子书签一起删
        if (idx == -1) {
            for (int c = 0; c < item->childCount(); ++c) {
                const int ci = item->child(c)->data(0, Qt::UserRole).toInt();
                if (ci >= 0)
                    toRemove << ci;
            }
        }
    }
    std::sort(toRemove.begin(), toRemove.end(), std::greater<int>());
    for (int idx : toRemove) {
        if (idx >= 0 && idx < m_bookmarks->size())
            m_bookmarks->removeAt(idx);
    }
    rebuild();
    emit changed();
}

void BookmarkManager::onRenameSelected()
{
    auto *item = m_tree->currentItem();
    if (!item)
        return;
    onItemDoubleClicked(item, 0);
}

void BookmarkManager::onItemMoved()
{
    if (!m_bookmarks)
        return;
    // 拖拽结束后，根据树结构重建书签列表（分组归属）
    QList<Bookmark> newList;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *top = m_tree->topLevelItem(i);
        const int idx = top->data(0, Qt::UserRole).toInt();
        if (idx == -1) {
            // 分组
            const QString g = top->data(0, Qt::UserRole + 1).toString();
            for (int c = 0; c < top->childCount(); ++c) {
                const int ci = top->child(c)->data(0, Qt::UserRole).toInt();
                if (ci >= 0 && ci < m_bookmarks->size()) {
                    Bookmark b = m_bookmarks->at(ci);
                    b.group = g;
                    newList.append(b);
                }
            }
        } else if (idx >= 0 && idx < m_bookmarks->size()) {
            Bookmark b = m_bookmarks->at(idx);
            b.group.clear();
            newList.append(b);
        }
    }
    // 保留未被树包含的（理论上不会发生）
    *m_bookmarks = newList;
    rebuild();
    emit changed();
}
