#ifndef SYNCDIALOG_H
#define SYNCDIALOG_H

#include <QDialog>

class QLineEdit;
class BrowserWindow;

// 云同步配置与操作对话框
class SyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncDialog(BrowserWindow *browser);

private slots:
    void onUpload();
    void onDownload();

private:
    void loadConfig();
    void saveConfig();

    BrowserWindow *m_browser = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passEdit = nullptr;
    QLineEdit *m_pathEdit = nullptr;
    QLineEdit *m_passphraseEdit = nullptr;
};

#endif // SYNCDIALOG_H
