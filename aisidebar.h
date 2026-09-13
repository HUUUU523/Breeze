#ifndef AISIDEBAR_H
#define AISIDEBAR_H

#include <QDockWidget>
#include <QJsonArray>

class QTextEdit;
class QLineEdit;
class QPushButton;
class AiManager;

// AI 侧边栏：常驻停靠面板，可带入当前页上下文
class AiSidebar : public QDockWidget
{
    Q_OBJECT

public:
    explicit AiSidebar(QWidget *parent = nullptr);

    // 设置当前页正文（作为后续提问的上下文）
    void setPageContext(const QString &title, const QString &text);

signals:
    // 请求获取当前页正文（由主窗口响应并回填 setPageContext）
    void pageContextRequested();

private slots:
    void onSend();
    void onChunk(const QString &delta);
    void onFinished(const QString &full);
    void onFailed(const QString &error);

private:
    void appendBubble(const QString &who, const QString &text, bool isUser);

    AiManager   *m_ai = nullptr;
    QTextEdit   *m_history = nullptr;
    QTextEdit   *m_input = nullptr;
    QPushButton *m_sendBtn = nullptr;
    QJsonArray   m_messages;
    bool         m_busy = false;
    QString      m_streaming;

    QString m_pageTitle;
    QString m_pageText;
};

#endif // AISIDEBAR_H
