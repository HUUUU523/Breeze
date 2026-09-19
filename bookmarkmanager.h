#ifndef BOOKMARKMANAGER_H
#define BOOKMARKMANAGER_H

#include <QDialog>
#include <QList>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
struct Bookmark;

// 书签管理对话框：树形显示分组/书签，支持拖拽移动、增删改
class BookmarkManager : public QDialog
{
    Q_OBJECT

public:
    explicit BookmarkManager(QWidget *parent = nullptr);

    // 通过引用直接操作主窗口的书签列表
    void setBookmarks(QList<Bookmark> *bookmarks);

signals:
    void changed();   // 书签被修改，主窗口应保存并刷新书签栏

private slots:
    void onFilterChanged(const QString &text);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onAddBookmark();
    void onAddGroup();
    void onRemoveSelected();
    void onRenameSelected();
    void onItemMoved();

private:
    void rebuild();
    QStringList allGroups() const;

    QTreeWidget *m_tree = nullptr;
    QLineEdit   *m_filter = nullptr;
    QLabel      *m_countLabel = nullptr;
    QList<Bookmark> *m_bookmarks = nullptr;
};

#endif // BOOKMARKMANAGER_H
