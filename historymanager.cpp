#include "historymanager.h"

#include <QApplication>
#include <QClipboard>
#include <QMenu>

#include <QBrush>
#include <QColor>
#include <QDate>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

HistoryDialog::HistoryDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("历史记录 - Breeze"));
    resize(720, 460);

    auto *layout = new QVBoxLayout(this);

    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("搜索："), this));
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText(QStringLiteral("按标题或网址过滤"));
    m_filter->setClearButtonEnabled(true);
    top->addWidget(m_filter);
    layout->addLayout(top);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    layout->addWidget(m_list);

    auto *bottom = new QHBoxLayout;
    auto *delBtn = new QPushButton(QStringLiteral("删除选中"), this);
    auto *clearBtn = new QPushButton(QStringLiteral("清空历史"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    bottom->addWidget(delBtn);
    bottom->addWidget(clearBtn);
    bottom->addStretch();
    bottom->addWidget(closeBtn);
    layout->addLayout(bottom);

    connect(m_filter, &QLineEdit::textChanged, this, &HistoryDialog::onFilterChanged);
    connect(m_list, &QListWidget::itemActivated, this, &HistoryDialog::onItemActivated);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested,
            this, &HistoryDialog::onContextMenu);
    connect(delBtn, &QPushButton::clicked, this, &HistoryDialog::onDeleteSelected);
    connect(clearBtn, &QPushButton::clicked, this, &HistoryDialog::onClearClicked);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
}

void HistoryDialog::setEntries(const QList<HistoryEntry> &entries)
{
    m_entries = entries;
    rebuild();
}

void HistoryDialog::addEntry(const HistoryEntry &entry)
{
    m_entries.prepend(entry);
    rebuild();
}

void HistoryDialog::onFilterChanged(const QString &text)
{
    Q_UNUSED(text);
    rebuild();
}

void HistoryDialog::rebuild()
{
    m_list->clear();
    const QString filter = m_filter->text().trimmed();

    // 按日期分组：今天 / 昨天 / yyyy-MM-dd
    const QDate today = QDate::currentDate();
    QString currentGroup;
    bool groupHeaderNeeded = true;

    for (const HistoryEntry &e : m_entries) {
        if (!filter.isEmpty()
            && !e.title.contains(filter, Qt::CaseInsensitive)
            && !e.url.toString().contains(filter, Qt::CaseInsensitive))
            continue;

        // 计算分组名
        const QDate d = e.visitedAt.date();
        QString groupName;
        if (d == today)
            groupName = QStringLiteral("今天");
        else if (d == today.addDays(-1))
            groupName = QStringLiteral("昨天");
        else
            groupName = d.toString(QStringLiteral("yyyy-MM-dd"));

        if (groupName != currentGroup) {
            currentGroup = groupName;
            auto *header = new QListWidgetItem(
                QStringLiteral("── %1 ──").arg(groupName), m_list);
            header->setFlags(Qt::NoItemFlags);          // 不可选
            header->setForeground(QBrush(QColor(120, 120, 120)));
            QFont f = header->font();
            f.setBold(true);
            header->setFont(f);
        }

        const QString time = e.visitedAt.toString(QStringLiteral("HH:mm"));
        const QString text = QStringLiteral("%1  |  %2\n%3")
                                 .arg(time, e.title.isEmpty() ? e.url.host() : e.title,
                                      e.url.toString());
        auto *item = new QListWidgetItem(text, m_list);
        item->setData(Qt::UserRole, e.url.toString());
        item->setToolTip(e.url.toString());
    }
    Q_UNUSED(groupHeaderNeeded);

    // 标题显示总条数
    setWindowTitle(QStringLiteral("历史记录（%1 条） - Breeze").arg(m_entries.size()));
}

void HistoryDialog::onContextMenu(const QPoint &pos)
{
    auto *item = m_list->itemAt(pos);
    if (!item)
        return;
    const QString stored = item->data(Qt::UserRole).toString();
    if (stored.isEmpty())
        return;   // 分组标题
    const QUrl url(stored);

    m_list->setCurrentItem(item);
    QMenu menu(this);
    QAction *open = menu.addAction(QStringLiteral("打开"));
    QAction *copy = menu.addAction(QStringLiteral("复制地址"));
    QAction *del  = menu.addAction(QStringLiteral("删除"));
    QAction *chosen = menu.exec(m_list->mapToGlobal(pos));
    if (chosen == open)
        emit openUrlRequested(url);
    else if (chosen == copy)
        QApplication::clipboard()->setText(url.toString());
    else if (chosen == del)
        onDeleteSelected();
}

void HistoryDialog::onItemActivated()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    const QString stored = item->data(Qt::UserRole).toString();
    if (stored.isEmpty())
        return;   // 日期分组标题
    const QUrl url(stored);
    if (url.isValid())
        emit openUrlRequested(url);
}

void HistoryDialog::onDeleteSelected()
{
    auto *item = m_list->currentItem();
    if (!item)
        return;
    const QString stored = item->data(Qt::UserRole).toString();
    if (stored.isEmpty())
        return;   // 分组标题
    const QUrl url(stored);

    // 从 m_entries 删除所有匹配该 URL 的条目
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries.at(i).url == url)
            m_entries.removeAt(i);
    }
    rebuild();
    emit entryRemoved(url);
}

void HistoryDialog::onClearClicked()
{
    if (QMessageBox::question(this, QStringLiteral("Breeze"),
                              QStringLiteral("确定清空全部历史记录吗？"))
        == QMessageBox::Yes) {
        m_entries.clear();
        rebuild();
        emit cleared();
    }
}
