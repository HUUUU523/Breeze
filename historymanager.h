#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <QDateTime>
#include <QDialog>
#include <QList>
#include <QUrl>

class QLineEdit;
class QListWidget;

// 单条历史记录
struct HistoryEntry {
    QString   title;
    QUrl      url;
    QDateTime visitedAt;
};

// 历史记录管理窗口
class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(QWidget *parent = nullptr);

    void setEntries(const QList<HistoryEntry> &entries);
    void addEntry(const HistoryEntry &entry);

signals:
    void openUrlRequested(const QUrl &url);
    void cleared();
    void entryRemoved(const QUrl &url);   // 删除单条

private slots:
    void onFilterChanged(const QString &text);
    void onItemActivated();
    void onClearClicked();
    void onDeleteSelected();

private:
    void rebuild();

    QList<HistoryEntry> m_entries;
    QLineEdit   *m_filter = nullptr;
    QListWidget *m_list   = nullptr;
};

#endif // HISTORYMANAGER_H
