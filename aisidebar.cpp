#include "aisidebar.h"
#include "aimanager.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QShortcut>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

AiSidebar::AiSidebar(QWidget *parent)
    : QDockWidget(QStringLiteral("AI 助手"), parent)
{
    setObjectName(QStringLiteral("AiSidebar"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *root = new QWidget(this);
    auto *v = new QVBoxLayout(root);

    m_history = new QTextEdit(root);
    m_history->setReadOnly(true);
    v->addWidget(m_history, 1);

    m_input = new QTextEdit(root);
    m_input->setPlaceholderText(QStringLiteral("输入问题，Ctrl+Enter 发送"));
    m_input->setMaximumHeight(80);
    v->addWidget(m_input);

    auto *row = new QHBoxLayout;
    m_sendBtn = new QPushButton(QStringLiteral("发送"), root);
    auto *clearBtn = new QPushButton(QStringLiteral("清空"), root);
    row->addWidget(m_sendBtn);
    row->addWidget(clearBtn);
    row->addStretch();
    v->addLayout(row);

    setWidget(root);

    m_ai = new AiManager(this);
    connect(m_ai, &AiManager::streamChunk,    this, &AiSidebar::onChunk);
    connect(m_ai, &AiManager::streamFinished, this, &AiSidebar::onFinished);
    connect(m_ai, &AiManager::failed,         this, &AiSidebar::onFailed);

    connect(m_sendBtn, &QPushButton::clicked, this, &AiSidebar::onSend);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_messages = QJsonArray();
        m_history->clear();
    });
    auto *sendSc = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), m_input);
    connect(sendSc, &QShortcut::activated, this, &AiSidebar::onSend);
}

void AiSidebar::setPageContext(const QString &title, const QString &text)
{
    m_pageTitle = title;
    m_pageText = text;
    if (!text.isEmpty())
        m_history->append(QStringLiteral("<i>已载入页面上下文：%1</i>")
            .arg(title.toHtmlEscaped()));
}

void AiSidebar::appendBubble(const QString &who, const QString &text, bool isUser)
{
    const QString color = isUser ? QStringLiteral("#3a6ea5") : QStringLiteral("#555");
    m_history->append(QStringLiteral("<b style='color:%1'>%2</b><br>%3<br>")
        .arg(color, who,
             text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
    m_history->verticalScrollBar()->setValue(m_history->verticalScrollBar()->maximum());
}

void AiSidebar::onSend()
{
    if (m_busy)
        return;
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    // 首次提问时带上页面上下文（若有）
    QString payload = text;
    if (!m_pageText.isEmpty() && m_messages.isEmpty()) {
        payload = QStringLiteral("以下是当前网页《%1》的正文：\n\n%2\n\n请基于此回答：%3")
            .arg(m_pageTitle, m_pageText.left(6000), text);
    }

    QJsonObject userMsg;
    userMsg.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMsg.insert(QStringLiteral("content"), payload);
    m_messages.append(userMsg);

    appendBubble(QStringLiteral("你"), text, true);
    m_input->clear();

    m_busy = true;
    m_sendBtn->setEnabled(false);
    m_sendBtn->setText(QStringLiteral("思考中…"));
    m_streaming.clear();

    m_ai->chatStream(m_messages);
}

void AiSidebar::onChunk(const QString &delta)
{
    m_streaming += delta;
    // 简化：流式阶段直接追加到最后（重绘整段太频繁）
    // 为避免复杂重绘，这里仅在完成时一次性显示
}

void AiSidebar::onFinished(const QString &full)
{
    QJsonObject aiMsg;
    aiMsg.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    aiMsg.insert(QStringLiteral("content"), full);
    m_messages.append(aiMsg);

    appendBubble(QStringLiteral("AI"), full, false);
    m_streaming.clear();

    m_busy = false;
    m_sendBtn->setEnabled(true);
    m_sendBtn->setText(QStringLiteral("发送"));
}

void AiSidebar::onFailed(const QString &error)
{
    appendBubble(QStringLiteral("AI"), QStringLiteral("[错误] ") + error, false);
    m_busy = false;
    m_sendBtn->setEnabled(true);
    m_sendBtn->setText(QStringLiteral("发送"));
}
