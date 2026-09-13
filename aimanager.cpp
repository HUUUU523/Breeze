#include "aimanager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <memory>

AiManager::AiManager(QObject *parent)
    : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

QString AiManager::endpoint()
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    return s.value(QStringLiteral("ai/endpoint"),
                   QStringLiteral("https://api.deepseek.com/v1/chat/completions")).toString();
}

QString AiManager::apiKey()
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    return s.value(QStringLiteral("ai/apiKey")).toString();
}

QString AiManager::model()
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    return s.value(QStringLiteral("ai/model"),
                   QStringLiteral("deepseek-chat")).toString();
}

void AiManager::setEndpoint(const QString &url)
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("ai/endpoint"), url.trimmed());
}

void AiManager::setApiKey(const QString &key)
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("ai/apiKey"), key.trimmed());
}

void AiManager::setModel(const QString &model)
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("ai/model"), model.trimmed());
}

void AiManager::fetchModels()
{
    QString ep = endpoint().trimmed();
    if (ep.isEmpty()) { emit modelsFetchFailed(QStringLiteral("未配置接口地址")); return; }

    // 去掉尾部 /chat/completions，得到 base
    if (ep.endsWith(QStringLiteral("/chat/completions")))
        ep.chop(int(qstrlen("/chat/completions")));
    while (ep.endsWith(QLatin1Char('/')))
        ep.chop(1);

    // 若 base 不含 /v1，且看起来是 Ollama（含 11434），用 /api/tags；否则用 /v1/models
    QString modelsUrl;
    if (!ep.contains(QStringLiteral("/v1")) && ep.contains(QStringLiteral("11434")))
        modelsUrl = ep + QStringLiteral("/api/tags");
    else
        modelsUrl = ep + QStringLiteral("/v1/models");

    QNetworkRequest req{QUrl(modelsUrl)};
    const QString key = apiKey();
    if (!key.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + key.toUtf8());

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit modelsFetchFailed(reply->errorString());
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        QStringList models;

        // OpenAI 风格：{ "data": [ { "id": "..." }, ... ] }
        const QJsonArray data = o.value(QStringLiteral("data")).toArray();
        for (const QJsonValue &v : data) {
            const QString id = v.toObject().value(QStringLiteral("id")).toString();
            if (!id.isEmpty()) models << id;
        }

        // Ollama 风格：{ "models": [ { "name": "..." }, ... ] }
        const QJsonArray arr = o.value(QStringLiteral("models")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject mo = v.toObject();
            QString name = mo.value(QStringLiteral("name")).toString();
            if (name.isEmpty()) name = mo.value(QStringLiteral("model")).toString();
            if (!name.isEmpty()) models << name;
        }

        models.removeDuplicates();
        if (models.isEmpty()) {
            emit modelsFetchFailed(QStringLiteral("未解析到模型列表（返回格式不匹配）"));
            return;
        }
        emit modelsFetched(models);
    });
}

void AiManager::chat(const QJsonArray &messages)
{
    sendRequest(messages, false);
}

void AiManager::chatStream(const QJsonArray &messages)
{
    sendRequest(messages, true);
}

void AiManager::cancelStream()
{
    if (m_streamReply) {
        m_streamReply->abort();
        m_streamReply->deleteLater();
        m_streamReply = nullptr;
    }
}

void AiManager::sendRequest(const QJsonArray &messages, bool stream)
{
    const QString ep = endpoint();
    const QString key = apiKey();
    const QString model = this->model();

    if (ep.isEmpty()) { emit failed(QStringLiteral("未配置 AI 接口地址")); return; }
    if (model.isEmpty()) { emit failed(QStringLiteral("未配置模型名称")); return; }

    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("messages"), messages);
    body.insert(QStringLiteral("stream"), stream);

    QNetworkRequest req{QUrl(ep)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!key.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + key.toUtf8());

    QNetworkReply *reply = m_net->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    if (!stream) {
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                const QByteArray raw = reply->readAll();
                QString detail = reply->errorString();
                if (!raw.isEmpty()) {
                    const QJsonObject o = QJsonDocument::fromJson(raw).object();
                    const QJsonObject err = o.value(QStringLiteral("error")).toObject();
                    if (err.contains(QStringLiteral("message")))
                        detail = err.value(QStringLiteral("message")).toString();
                }
                emit failed(detail);
                return;
            }
            const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonArray choices = o.value(QStringLiteral("choices")).toArray();
            if (choices.isEmpty()) { emit failed(QStringLiteral("接口返回内容为空")); return; }
            const QString content = choices.first().toObject()
                .value(QStringLiteral("message")).toObject()
                .value(QStringLiteral("content")).toString();
            if (content.isEmpty()) { emit failed(QStringLiteral("接口未返回文本")); return; }
            emit finished(content);
        });
        return;
    }

    // ---- 流式（SSE） ----
    m_streamReply = reply;
    auto full = std::make_shared<QString>();
    auto buffer = std::make_shared<QByteArray>();

    connect(reply, &QNetworkReply::readyRead, this, [this, reply, buffer, full]() {
        *buffer += reply->readAll();
        int pos;
        while ((pos = buffer->indexOf('\n')) >= 0) {
            QByteArray line = buffer->left(pos);
            buffer->remove(0, pos + 1);
            line = line.trimmed();
            if (!line.startsWith("data:"))
                continue;
            const QByteArray payload = line.mid(5).trimmed();
            if (payload == "[DONE]")
                continue;
            const QJsonObject o = QJsonDocument::fromJson(payload).object();
            const QJsonArray choices = o.value(QStringLiteral("choices")).toArray();
            if (choices.isEmpty())
                continue;
            const QJsonObject delta = choices.first().toObject()
                .value(QStringLiteral("delta")).toObject();
            const QString piece = delta.value(QStringLiteral("content")).toString();
            if (!piece.isEmpty()) {
                *full += piece;
                emit streamChunk(piece);
            }
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, full]() {
        reply->deleteLater();
        if (m_streamReply == reply)
            m_streamReply = nullptr;
        if (reply->error() != QNetworkReply::NoError
            && reply->error() != QNetworkReply::OperationCanceledError) {
            emit failed(reply->errorString());
            return;
        }
        emit streamFinished(*full);
    });
}