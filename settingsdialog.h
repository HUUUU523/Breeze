#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;

// 设置对话框：主页、默认搜索引擎、隐私（是否记录历史）
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // 读取/写入配置（静态，全局可用）
    static QString homePage();
    static QString searchEngine();
    static bool    recordHistory();
    static void    setHomePage(const QString &url);
    static void    setSearchEngine(const QString &engine);
    static void    setRecordHistory(bool enabled);

    // 搜索引擎名 -> 查询 URL 模板（含 %1 占位）
    static QString searchUrlTemplate(const QString &engineName);

private slots:
    void onAccepted();

private:
    void load();

    QLineEdit *m_homeEdit    = nullptr;
    QComboBox *m_engineCombo = nullptr;
    QCheckBox *m_historyCheck = nullptr;
};

#endif // SETTINGSDIALOG_H
