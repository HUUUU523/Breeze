#ifndef USERSCRIPTMANAGER_H
#define USERSCRIPTMANAGER_H

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

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

    // ---- UserScript 元数据 ----
    QString description;
    QString runAt = QStringLiteral("document-idle");  // document-start / document-end / document-idle
    QStringList grants;     // @grant 列表
    QStringList requires;   // @require URL 列表
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

    // 解析脚本头部的 ==UserScript== 元数据块，填充 name/match/runAt/grant/require 等
    // 若没有元数据块则不修改（保持已有字段）
    static void parseMetadata(UserScript &script);

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
