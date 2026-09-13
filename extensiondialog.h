#ifndef EXTENSIONDIALOG_H
#define EXTENSIONDIALOG_H

#include <QDialog>

class QTableWidget;

// 扩展管理：列出已加载扩展、添加/移除目录
class ExtensionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExtensionDialog(QWidget *parent = nullptr);

private slots:
    void onAdd();
    void onRemove();
    void reload();

private:
    QTableWidget *m_table = nullptr;
};

#endif // EXTENSIONDIALOG_H
