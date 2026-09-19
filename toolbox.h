#ifndef TOOLBOX_H
#define TOOLBOX_H

#include <QDialog>

class QTabWidget;
class QTextEdit;
class QLineEdit;

// 工具箱：JSON、URL、Base64、时间戳、密码、颜色、单位、取色器、二维码
class ToolboxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ToolboxDialog(QWidget *parent = nullptr);

private:
    QWidget *createJsonTab();
    QWidget *createEncodeTab();
    QWidget *createTimeTab();
    QWidget *createPasswordTab();
    QWidget *createColorTab();
    QWidget *createUnitTab();
    QWidget *createQrTab();
    QWidget *createHashTab();
    QWidget *createTextStatTab();
    QWidget *createRegexTab();

    // 二维码：用 QPainter 画（不依赖第三方库）
    static QPixmap makeQrPixmap(const QString &text, int size = 260);
};

#endif // TOOLBOX_H
