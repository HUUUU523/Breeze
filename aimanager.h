#ifndef AIMANAGER_H
#define AIMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonArray>

class QNetworkAccessManager;
class QNetworkReply;

// OpenAI 兼容接口客户端（/v1/chat/completions）
// 兼容 OpenAI、DeepSeek、通义千问、Kimi、Ollama 等
class AiManager : public QObject
{
    Q_OBJECT

public:
    explicit AiManager(QObject *parent = nullptr);

    // 配置读写（QSettings）
    static QString endpoint();
    static QString apiKey();
    static QString model();
    static void setEndpoint(const QString &url);
    static void setApiKey(const QString &key);
    static void setModel(const QString &model);

    // 发送对话（非流式）；messages 为 [{role,content}, ...]
    void chat(const QJsonArray &messages);

    // 发送对话（流式），增量通过 chunkReceived 回调
    void chatStream(const QJsonArray &messages);

    // 取消当前流式请求
    void cancelStream();

    // 拉取可用模型列表（OpenAI /models 或 Ollama /api/tags）
    void fetchModels();

signals:
    void finished(const QString &content);         // 非流式：完整结果
    void streamChunk(const QString &delta);        // 流式：增量片段
    void streamFinished(const QString &fullText);  // 流式：完成
    void failed(const QString &error);
    void modelsFetched(const QStringList &models);
    void modelsFetchFailed(const QString &error);

private:
    void sendRequest(const QJsonArray &messages, bool stream);

    QNetworkAccessManager *m_net = nullptr;
    QNetworkReply *m_streamReply = nullptr;
};

#endif // AIMANAGER_H
