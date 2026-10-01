#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>

class QListWidget;
class QPushButton;

// 本地多用户管理：新建/删除/切换用户配置。
// 每个用户有独立的书签/历史/设置/会话目录。
class ProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProfileDialog(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onNewProfile();
    void onDeleteProfile();
    void onSwitch();

private:
    QListWidget *m_list = nullptr;
    QPushButton *m_switchBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
};

#endif // PROFILEDIALOG_H
