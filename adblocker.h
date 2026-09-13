#ifndef ADBLOCKER_H
#define ADBLOCKER_H

#include <QObject>
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

    // 添加/移除自定义规则
    void addRule(const QString &rule);
    void removeRule(const QString &rule);
    QStringList customRules() const { return m_customRules; }

    // 拦截计数
    int blockedCount() const { return m_blockedCount; }
    void resetCount() { m_blockedCount = 0; }

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

private:
    bool shouldBlock(const QUrl &url) const;
    void loadBuiltinRules();

    bool m_enabled = true;
    QSet<QString> m_domains;      // 域名黑名单
    QStringList m_customRules;    // 自定义规则
    QStringList m_keywords;       // URL 关键字
    mutable int m_blockedCount = 0;
};

#endif // ADBLOCKER_H
