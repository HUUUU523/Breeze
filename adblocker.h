#ifndef ADBLOCKER_H
#define ADBLOCKER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QWebEngineUrlRequestInterceptor>

// 广告拦截器：基于域名黑名单 + 关键字规则
class AdBlocker : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

public:
    explicit AdBlocker(QObject *parent = nullptr);

    // 启用/禁用
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // HTTPS 强制升级（http 主框架请求重定向到 https）
    void setHttpsUpgrade(bool enabled);
    bool httpsUpgrade() const { return m_httpsUpgrade; }

    // 添加/移除自定义规则
    void addRule(const QString &rule);
    void removeRule(const QString &rule);
    QStringList customRules() const { return m_customRules; }

    // 拦截计数（全局）
    int blockedCount() const { return m_blockedCount; }
    void resetCount() { m_blockedCount = 0; }

    // 某站点（firstPartyUrl 的 host）被拦截的数量
    int blockedCountForHost(const QString &host) const;

    // 某站点最近被拦截的 URL 列表（去重，最多 50 条）
    QStringList blockedUrlsForHost(const QString &host) const;

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

signals:
    // 某站点拦截计数变化
    void blockedCountChanged(const QString &host, int count);

private:
    bool shouldBlock(const QUrl &url) const;
    void loadBuiltinRules();

    bool m_enabled = true;
    bool m_httpsUpgrade = false;
    QSet<QString> m_domains;      // 域名黑名单
    QStringList m_customRules;    // 自定义规则
    QStringList m_keywords;       // URL 关键字
    mutable int m_blockedCount = 0;

    QHash<QString, int> m_perHost;            // host -> 拦截数
    QHash<QString, QStringList> m_perHostUrls; // host -> 被拦截 URL
};

#endif // ADBLOCKER_H
