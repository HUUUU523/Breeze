#ifndef TASKMANAGERDIALOG_H
#define TASKMANAGERDIALOG_H

#include <QDialog>
#include <QList>

class QTableWidget;
class QTimer;
class WebView;

// 简易任务管理器：列出各标签的标题/URL/渲染进程 PID/内存占用。
class TaskManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TaskManagerDialog(QWidget *parent = nullptr);

    // 每次刷新时由主窗口提供当前标签列表
    void setTabs(const QList<WebView *> &tabs);

private:
    void refresh();
    QTableWidget *m_table = nullptr;
    QList<WebView *> m_tabs;
    QTimer *m_timer = nullptr;
};

#endif // TASKMANAGERDIALOG_H
