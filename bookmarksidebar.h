#ifndef BOOKMARKSIDEBAR_H
#define BOOKMARKSIDEBAR_H

#include <QDockWidget>

class QLineEdit;
class QListWidget;
class QTabWidget;

// 侧边栏：书签 / 历史 两个面板
class BookmarkSidebar : public QDockWidget
{
    Q_OBJECT

public:
    explicit BookmarkSidebar(QWidget *parent = nullptr);

    // 由主窗口在数据变化时调用
    void setBookmarks(const QList<QPair<QString, QUrl>> &bookmarks);
    void setHistory(const QList<QPair<QString, QUrl>> &history);

signals:
    void urlActivated(const QUrl &url);

private slots:
    void onFilterChanged(const QString &text);

private:
    void rebuildBookmarkList();
    void rebuildHistoryList();

    QTabWidget  *m_tabs = nullptr;
    QLineEdit   *m_bmFilter = nullptr;
    QListWidget *m_bmList = nullptr;
    QLineEdit   *m_hisFilter = nullptr;
    QListWidget *m_hisList = nullptr;

    QList<QPair<QString, QUrl>> m_bookmarks;
    QList<QPair<QString, QUrl>> m_history;
};

#endif // BOOKMARKSIDEBAR_H
