#include "updatemanager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {
const char *kApiUrl = "https://api.github.com/repos/HUUUU523/Breeze/releases/latest";

// 去掉前缀 v/V，便于比较
QString normalizeVersion(QString v)
{
    v = v.trimmed();
    if (v.startsWith(QLatin1Char('v')) || v.startsWith(QLatin1Char('V')))
        v = v.mid(1);
    return v;
}
}

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
{
    m_net = new QNetworkAccessManager(this);
}

void UpdateManager::checkForUpdates()
{
    QNetworkRequest req{QUrl(QString::fromLatin1(kApiUrl))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Breeze-Updater"));
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }

        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = o.value(QStringLiteral("tag_name")).toString();
        const QString pageUrl = o.value(QStringLiteral("html_url")).toString();
        const QString body = o.value(QStringLiteral("body")).toString();

        if (tag.isEmpty()) {
            emit checkFailed(QStringLiteral("未获取到版本信息"));
            return;
        }

        const QString latest = normalizeVersion(tag);
        const QString current = normalizeVersion(QStringLiteral(BREEZE_VERSION));

        if (latest == current) {
            emit upToDate(current);
            return;
        }

        // 比较三段版本号
        auto parse = [](const QString &s) {
            QList<int> parts;
            for (const QString &p : s.split(QLatin1Char('.')))
                parts << p.toInt();
            while (parts.size() < 3) parts << 0;
            return parts;
        };
        const QList<int> a = parse(latest);
        const QList<int> b = parse(current);
        const bool newer = (a[0] > b[0])
            || (a[0] == b[0] && a[1] > b[1])
            || (a[0] == b[0] && a[1] == b[1] && a[2] > b[2]);

        if (newer)
            emit updateAvailable(latest, pageUrl, body);
        else
            emit upToDate(current);
    });
}
