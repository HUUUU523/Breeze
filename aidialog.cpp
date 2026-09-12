#include "aidialog.h"
#include "aimanager.h"

#include <QFormLayout>
#include <QJsonObject>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>

AiDialog::AiDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("AI 助手 - Breeze"));
    resize(760, 620);

    m_ai = new AiManager(this);
    connect(m_ai, &AiManager::finished,       this, &AiDialog::onFinished);
    connect(m_ai, &AiManager::failed,         this, &AiDialog::onFailed);
    connect(m_ai, &AiManager::streamChunk,    this, &AiDialog::onStreamChunk);
    connect(m_ai, &AiManager::streamFinished, this, &AiDialog::onStreamFinished);

    auto *layout = new QVBoxLayout(this);

    // 历史显示
    m_history = new QTextEdit(this);
    m_history->setReadOnly(true);
    layout->addWidget(m_history, 1);

    // 输入
    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(QStringLiteral("输入问题，Ctrl+Enter 发送"));
    m_input->setMaximumHeight(90);
    layout->addWidget(m_input);

    auto *btnRow = new QHBoxLayout;
    m_sendBtn = new QPushButton(QStringLiteral("发送"), this);
    auto *clearBtn = new QPushButton(QStringLiteral("清空对话"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    btnRow->addWidget(m_sendBtn);
    btnRow->addWidget(clearBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    // 配置区
    auto *cfg = new QGroupBox(QStringLiteral("接口配置（OpenAI 兼容）"), this);
    auto *form = new QFormLayout(cfg);
    m_endpointEdit = new QLineEdit(cfg);
    m_keyEdit = new QLineEdit(cfg);
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_modelEdit = new QLineEdit(cfg);
    form->addRow(QStringLiteral("接口地址："), m_endpointEdit);
    form->addRow(QStringLiteral("API Key："), m_keyEdit);
    form->addRow(QStringLiteral("模型："), m_modelEdit);
    layout->addWidget(cfg);

    refreshSettingsBar();

    connect(m_sendBtn, &QPushButton::clicked, this, &AiDialog::onSend);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_messages = QJsonArray();
        m_history->clear();
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    // 配置变化即保存
    auto saveCfg = [this]() {
        AiManager::setEndpoint(m_endpointEdit->text());
        AiManager::setApiKey(m_keyEdit->text());
        AiManager::setModel(m_modelEdit->text());
    };
    connect(m_endpointEdit, &QLineEdit::editingFinished, this, saveCfg);
    connect(m_keyEdit,      &QLineEdit::editingFinished, this, saveCfg);
    connect(m_modelEdit,    &QLineEdit::editingFinished, this, saveCfg);
}

void AiDialog::refreshSettingsBar()
{
    m_endpointEdit->setText(AiManager::endpoint());
    m_keyEdit->setText(AiManager::apiKey());
    m_modelEdit->setText(AiManager::model());
}

void AiDialog::askWithPrompt(const QString &prompt)
{
    m_input->setPlainText(prompt);
    onSend();
}

void AiDialog::redraw()
{
    m_history->clear();
    for (const QJsonValue &v : m_messages) {
        const QJsonObject o = v.toObject();
        const QString role = o.value(QStringLiteral("role")).toString();
        const QString content = o.value(QStringLiteral("content")).toString();
        const QString who = (role == QStringLiteral("user"))
            ? QStringLiteral("你") : QStringLiteral("AI");
        m_history->append(QStringLiteral("<b>%1</b><br>%2<br>")
            .arg(who, content.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
    }
    // 流式临时内容（尚未写入 m_messages）
    if (!m_streamingText.isEmpty()) {
        m_history->append(QStringLiteral("<b>AI</b><br>%1<br>")
            .arg(m_streamingText.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
    }
    m_history->verticalScrollBar()->setValue(m_history->verticalScrollBar()->maximum());
}

void AiDialog::onSend()
{
    if (m_busy)
        return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    // 保存配置
    AiManager::setEndpoint(m_endpointEdit->text());
    AiManager::setApiKey(m_keyEdit->text());
    AiManager::setModel(m_modelEdit->text());

    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMsg.insert(QStringLiteral("content"), text);
    m_messages.append(userMsg);

    // 限制历史长度
    while (m_messages.size() > 20)
        m_messages.removeFirst();

    m_input->clear();
    m_busy = true;
    m_sendBtn->setEnabled(false);
    m_sendBtn->setText(QStringLiteral("思考中…"));

    m_streamingText.clear();
    redraw();                     // 只画一次：含用户消息
    m_ai->chatStream(m_messages);
}

void AiDialog::onStreamChunk(const QString &delta)
{
    m_streamingText += delta;
    redraw();                     // 重画：历史 + 流式内容
}

void AiDialog::onStreamFinished(const QString &full)
{
    if (full.isEmpty()) {
        onFailed(QStringLiteral("接口未返回文本"));
        return;
    }
    QJsonObject aiMsg;
    aiMsg.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    aiMsg.insert(QStringLiteral("content"), full);
    m_messages.append(aiMsg);

    m_streamingText.clear();
    redraw();                     // 流式内容已进 m_messages，重画一次即可

    m_busy = false;
    m_sendBtn->setEnabled(true);
    m_sendBtn->setText(QStringLiteral("发送"));
}

void AiDialog::onFinished(const QString &content)
{
    QJsonObject aiMsg;
    aiMsg.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    aiMsg.insert(QStringLiteral("content"), content);
    m_messages.append(aiMsg);
    redraw();

    m_busy = false;
    m_sendBtn->setEnabled(true);
    m_sendBtn->setText(QStringLiteral("发送"));
}

void AiDialog::onFailed(const QString &error)
{
    QJsonObject errMsg;
    errMsg.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    errMsg.insert(QStringLiteral("content"), QStringLiteral("[错误] ") + error);
    m_messages.append(errMsg);
    m_streamingText.clear();
    redraw();

    m_busy = false;
    m_sendBtn->setEnabled(true);
    m_sendBtn->setText(QStringLiteral("发送"));
}