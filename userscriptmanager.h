#ifndef USERSCRIPTMANAGER_H
#define USERSCRIPTMANAGER_H

#include <QDialog>
#include <QList>
#include <QString>

class QTableWidget;
class QLineEdit;
class QTextEdit;
class QCheckBox;

// 一条用户脚本
struct UserScript {
    QString name;
    QString match;      // URL 匹配（支持 * 通配），如 *://*.example.com/*
    QString code;       // JavaScript 或 CSS
    bool    enabled = true;
    bool    isCss = false;   // true 表示 code 是 CSS
};

// 用户脚本管理对话框
class UserScriptManager : public QDialog
{
    Q_OBJECT

public:
    explicit UserScriptManager(QWidget *parent = nullptr);

    static QList<UserScript> loadScripts();
    static void saveScripts(const QList<UserScript> &scripts);

    // 判断脚本的 match 是否匹配某 URL（供 WebView 调用）
    static bool matchesUrl(const UserScript &script, const QUrl &url);

private slots:
    void onAdd();
    void onEdit();
    void onRemove();

private:
    void reload();
    int currentRow() const;

    QTableWidget *m_table = nullptr;
    QList<UserScript> m_scripts;
};

#endif // USERSCRIPTMANAGER_H
