#ifndef COOKIEMANAGERDIALOG_H
#define COOKIEMANAGERDIALOG_H

#include <QDialog>
#include <QList>
#include <QNetworkCookie>

class QTableWidget;
class QWebEngineCookieStore;

// Cookie 管理：列出、删除
class CookieManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CookieManagerDialog(QWidget *parent = nullptr);

private slots:
    void onDeleteSelected();
    void onDeleteAll();

private:
    void reload();
    int currentRow() const;

    QTableWidget *m_table = nullptr;
    QWebEngineCookieStore *m_store = nullptr;
    QList<QNetworkCookie> m_cookies;
    bool m_loading = false;
};

#endif // COOKIEMANAGERDIALOG_H
