#ifndef READINGLISTMANAGER_H
#define READINGLISTMANAGER_H

#include <QDateTime>
#include <QDialog>
#include <QList>
#include <QUrl>

class QListWidget;
class QPushButton;
class QLabel;

// 单条稍后读
struct ReadingItem {
    QString   title;
    QUrl      url;
    QDateTime addedAt;
    bool      read = false;
};

// 稍后读列表对话框
class ReadingListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReadingListDialog(QWidget *parent = nullptr);

    void setItems(const QList<ReadingItem> &items);

signals:
    void openUrlRequested(const QUrl &url);
    void toggleReadRequested(const QUrl &url);   // 切换已读
    void removeRequested(const QUrl &url);

private slots:
    void onItemActivated();
    void onToggleRead();
    void onRemove();
    void onContextMenu(const QPoint &pos);

private:
    void rebuild();

    QList<ReadingItem> m_items;
    QListWidget *m_list = nullptr;
    QLabel      *m_stat = nullptr;
};

#endif // READINGLISTMANAGER_H
