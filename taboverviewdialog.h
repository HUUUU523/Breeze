#ifndef TABOVERVIEWDIALOG_H
#define TABOVERVIEWDIALOG_H

#include <QDialog>
#include <QList>

class QTabWidget;

// 标签页缩略图网格总览（Safari/Edge 风格）
class TabOverviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TabOverviewDialog(QTabWidget *tabs, QWidget *parent = nullptr);

signals:
    void tabActivated(int index);
    void tabClosed(int index);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTabWidget *m_tabs = nullptr;
};

#endif // TABOVERVIEWDIALOG_H
