#include "bookmarksidebar.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

BookmarkSidebar::BookmarkSidebar(QWidget *parent)
    : QDockWidget(QStringLiteral("书签 / 历史"), parent)
{
    setObjectName(QStringLiteral("BookmarkSidebar"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_tabs = new QTabWidget(this);

    // ---- 书签页 ----
    auto *bmPage = new QWidget(this);
    auto *bmLayout = new QVBoxLayout(bmPage);
    m_bmFilter = new QLineEdit(bmPage);
    m_bmFilter->setPlaceholderText(QStringLiteral("筛选书签…"));
    m_bmFilter->setClearButtonEnabled(true);
    m_bmList = new QListWidget(bmPage);
    bmLayout->addWidget(m_bmFilter);
    bmLayout->addWidget(m_bmList, 1);
    m_tabs->addTab(bmPage, QStringLiteral("书签"));

    // ---- 历史页 ----
    auto *hisPage = new QWidget(this);
    auto *hisLayout = new QVBoxLayout(hisPage);
    m_hisFilter = new QLineEdit(hisPage);
    m_hisFilter->setPlaceholderText(QStringLiteral("筛选历史…"));
    m_hisFilter->setClearButtonEnabled(true);
    m_hisList = new QListWidget(hisPage);
    hisLayout->addWidget(m_hisFilter);
    hisLayout->addWidget(m_hisList, 1);
    m_tabs->addTab(hisPage, QStringLiteral("历史"));

    setWidget(m_tabs);

    connect(m_bmFilter, &QLineEdit::textChanged, this, &BookmarkSidebar::onFilterChanged);
    connect(m_hisFilter, &QLineEdit::textChanged, this, &BookmarkSidebar::onFilterChanged);

    auto activate = [this](QListWidget *list) {
        connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            if (item)
                emit urlActivated(item->data(Qt::UserRole).toUrl());
        });
        connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
            if (item)
                emit urlActivated(item->data(Qt::UserRole).toUrl());
        });
    };
    activate(m_bmList);
    activate(m_hisList);
}

void BookmarkSidebar::setBookmarks(const QList<QPair<QString, QUrl>> &bookmarks)
{
    m_bookmarks = bookmarks;
    rebuildBookmarkList();
}

void BookmarkSidebar::setHistory(const QList<QPair<QString, QUrl>> &history)
{
    m_history = history;
    rebuildHistoryList();
}

void BookmarkSidebar::onFilterChanged(const QString &text)
{
    Q_UNUSED(text);
    rebuildBookmarkList();
    rebuildHistoryList();
}

void BookmarkSidebar::rebuildBookmarkList()
{
    const QString filter = m_bmFilter->text().trimmed();
    m_bmList->clear();
    for (const auto &p : m_bookmarks) {
        if (!filter.isEmpty()
            && !p.first.contains(filter, Qt::CaseInsensitive)
            && !p.second.toString().contains(filter, Qt::CaseInsensitive))
            continue;
        auto *item = new QListWidgetItem(p.first.isEmpty() ? p.second.toString() : p.first);
        item->setToolTip(p.second.toString());
        item->setData(Qt::UserRole, p.second);
        m_bmList->addItem(item);
    }
}

void BookmarkSidebar::rebuildHistoryList()
{
    const QString filter = m_hisFilter->text().trimmed();
    m_hisList->clear();
    for (const auto &p : m_history) {
        if (!filter.isEmpty()
            && !p.first.contains(filter, Qt::CaseInsensitive)
            && !p.second.toString().contains(filter, Qt::CaseInsensitive))
            continue;
        auto *item = new QListWidgetItem(p.first.isEmpty() ? p.second.toString() : p.first);
        item->setToolTip(p.second.toString());
        item->setData(Qt::UserRole, p.second);
        m_hisList->addItem(item);
    }
}
