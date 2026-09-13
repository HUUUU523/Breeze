#include "adblocker.h"

#include <QSettings>
#include <QUrl>

AdBlocker::AdBlocker(QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
    loadBuiltinRules();

    // 读取开关和自定义规则
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    m_enabled = s.value(QStringLiteral("adblock/enabled"), true).toBool();
    m_httpsUpgrade = s.value(QStringLiteral("adblock/httpsUpgrade"), false).toBool();
    m_customRules = s.value(QStringLiteral("adblock/rules")).toStringList();
    for (const QString &r : m_customRules)
        m_domains.insert(r.toLower());
}

void AdBlocker::loadBuiltinRules()
{
    // 常见广告/追踪/统计域名（精简版）
    static const char *builtin[] = {
        // 广告网络
        "doubleclick.net", "googlesyndication.com", "googleadservices.com",
        "google-analytics.com", "googletagmanager.com", "googletagservices.com",
        "adservice.google.com", "pagead2.googlesyndication.com",
        "adnxs.com", "adsrvr.org", "advertising.com", "adcolony.com",
        "criteo.com", "criteo.net", "outbrain.com", "taboola.com",
        "pubmatic.com", "rubiconproject.com", "openx.net", "casalemedia.com",
        "smartadserver.com", "adform.net", "media.net", "bidswitch.net",
        "serving-sys.com", "2mdn.net", "moatads.com", "admaster.com.cn",
        // 国内广告
        "pos.baidu.com", "cpro.baidu.com", "hm.baidu.com", "union.baidu.com",
        "tanx.com", "alimama.com", "mmstat.com", "cnzz.com",
        "umeng.com", "umengcloud.com", "gdt.qq.com", "e.qq.com",
        "adsmind.apdcdn.tc.qq.com", "pingjs.qq.com", "pingma.qq.com",
        "irs01.com", "miaozhen.com", "admaster.com.cn", "weibo.com/ajax",
        // 追踪/统计
        "scorecardresearch.com", "quantserve.com", "hotjar.com",
        "mixpanel.com", "segment.io", "amplitude.com", "fullstory.com",
        "mouseflow.com", "crazyegg.com", "clarity.ms", "newrelic.com",
        "sentry.io", "bugsnag.com", "raygun.io",
        // 社交追踪
        "connect.facebook.net", "platform.twitter.com", "platform.linkedin.com",
        "analytics.tiktok.com", "business-api.tiktok.com",
        nullptr
    };
    for (int i = 0; builtin[i]; ++i)
        m_domains.insert(QString::fromLatin1(builtin[i]));

    // URL 关键字（路径/查询串里含这些词视为广告）
    m_keywords = {
        "/ads/", "/ad/", "/adv/", "/advert", "/banner/", "/popup/",
        "/sponsor", "/tracking/", "/track/", "/analytics/", "/stats/",
        "/beacon/", "/collect/", "/telemetry/", "advertisement",
        "/pagead/", "/adserver/", "/adframe/", "/adimage",
    };
}

void AdBlocker::setEnabled(bool enabled)
{
    m_enabled = enabled;
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("adblock/enabled"), enabled);
}

void AdBlocker::setHttpsUpgrade(bool enabled)
{
    m_httpsUpgrade = enabled;
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("adblock/httpsUpgrade"), enabled);
}

void AdBlocker::addRule(const QString &rule)
{
    const QString r = rule.trimmed().toLower();
    if (r.isEmpty() || m_customRules.contains(r))
        return;
    m_customRules.append(r);
    m_domains.insert(r);
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("adblock/rules"), m_customRules);
}

void AdBlocker::removeRule(const QString &rule)
{
    const QString r = rule.trimmed().toLower();
    m_customRules.removeAll(r);
    // 内置规则不移除（仅移除自定义）
    m_domains.remove(r);
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("adblock/rules"), m_customRules);
}

bool AdBlocker::shouldBlock(const QUrl &url) const
{
    const QString host = url.host().toLower();
    if (host.isEmpty())
        return false;

    // 域名匹配（含子域）
    for (const QString &d : m_domains) {
        if (host == d || host.endsWith(QLatin1Char('.') + d))
            return true;
    }

    // URL 关键字匹配
    const QString full = url.toString().toLower();
    for (const QString &kw : m_keywords) {
        if (full.contains(kw))
            return true;
    }
    return false;
}

void AdBlocker::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (!m_enabled)
        return;

    const QUrl url = info.requestUrl();
    // 只拦截 http/https，放行本地和主文档
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return;

    // HTTPS 强制升级：主框架 http 请求重定向到 https
    if (m_httpsUpgrade && scheme == QLatin1String("http")
        && info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame) {
        QUrl httpsUrl = url;
        httpsUrl.setScheme(QStringLiteral("https"));
        info.redirect(httpsUrl);
        return;
    }

    // 放行主框架导航（避免误杀整站）
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame)
        return;

    if (shouldBlock(url)) {
        info.block(true);
        ++m_blockedCount;

        // 归属到 first party host（顶层页面）
        const QString host = info.firstPartyUrl().host().toLower();
        if (!host.isEmpty()) {
            ++m_perHost[host];
            QStringList &lst = m_perHostUrls[host];
            const QString u = url.toString();
            if (!lst.contains(u)) {
                lst.prepend(u);
                while (lst.size() > 50)
                    lst.removeLast();
            }
            emit blockedCountChanged(host, m_perHost.value(host));
        }
    }
}

int AdBlocker::blockedCountForHost(const QString &host) const
{
    return m_perHost.value(host.toLower(), 0);
}

QStringList AdBlocker::blockedUrlsForHost(const QString &host) const
{
    return m_perHostUrls.value(host.toLower());
}
