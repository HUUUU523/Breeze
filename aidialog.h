#ifndef AIDIALOG_H
#define AIDIALOG_H

#include <QDialog>
#include <QJsonArray>

class QTextEdit;
class QLineEdit;
class QPushButton;
class AiManager;

// AI 对话窗口：多轮对话
class AiDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AiDialog(QWidget *parent = nullptr);

    // 预填一段提示（用于网页总结）
    void askWithPrompt(const QString &prompt);

private slots:
    void onSend();
    void onFinished(const QString &content);
    void onFailed(const QString &error);
    void onStreamChunk(const QString &delta);
    void onStreamFinished(const QString &full);

private:
    void redraw();                // 按 m_messages + 流式内容完整重绘
    void refreshSettingsBar();

    AiManager   *m_ai = nullptr;
    QTextEdit   *m_history = nullptr;
    QTextEdit   *m_input = nullptr;
    QPushButton *m_sendBtn = nullptr;
    QLineEdit   *m_endpointEdit = nullptr;
    QLineEdit   *m_keyEdit = nullptr;
    QLineEdit   *m_modelEdit = nullptr;
    QJsonArray   m_messages;
    bool         m_busy = false;
    QString      m_streamingText;   // 当前流式回答累积
};

#endif // AIDIALOG_H
