#include "translator.h"

Translator *Translator::instance()
{
    static Translator inst;
    return &inst;
}

Translator::Translator()
{
    buildDictionaries();
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    m_lang = s.value(QStringLiteral("i18n/language"), QStringLiteral("zh")).toString();
}

QString Translator::language() const
{
    return m_lang;
}

void Translator::setLanguage(const QString &lang)
{
    const QString v = (lang == QStringLiteral("en")) ? QStringLiteral("en") : QStringLiteral("zh");
    if (v == m_lang)
        return;
    m_lang = v;
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("i18n/language"), m_lang);
    emit languageChanged();
}

QString Translator::t(const QString &key) const
{
    if (m_lang == QStringLiteral("en")) {
        const auto it = m_en.constFind(key);
        return it != m_en.constEnd() ? it.value() : m_zh.value(key, key);
    }
    return m_zh.value(key, key);
}

void Translator::buildDictionaries()
{
    // 中文（基准）
    m_zh = {
        {QStringLiteral("app.title"),        QStringLiteral("Breeze")},
        {QStringLiteral("nav.back"),         QStringLiteral("后退")},
        {QStringLiteral("nav.forward"),      QStringLiteral("前进")},
        {QStringLiteral("nav.reload"),       QStringLiteral("刷新")},
        {QStringLiteral("nav.stop"),         QStringLiteral("停止")},
        {QStringLiteral("nav.home"),         QStringLiteral("主页")},
        {QStringLiteral("nav.newTab"),       QStringLiteral("新建标签页")},
        {QStringLiteral("nav.newPrivate"),   QStringLiteral("新建隐私标签页")},
        {QStringLiteral("url.placeholder"),  QStringLiteral("搜索或输入网址")},
        {QStringLiteral("bookmark.add"),     QStringLiteral("添加书签")},
        {QStringLiteral("bookmark.manage"),  QStringLiteral("书签管理")},
        {QStringLiteral("downloads"),        QStringLiteral("下载管理")},
        {QStringLiteral("history"),          QStringLiteral("历史记录")},
        {QStringLiteral("settings"),         QStringLiteral("设置")},
        {QStringLiteral("sync"),             QStringLiteral("云同步")},
        {QStringLiteral("userscript"),       QStringLiteral("用户脚本")},
        {QStringLiteral("theme"),            QStringLiteral("主题")},
        {QStringLiteral("theme.system"),     QStringLiteral("跟随系统")},
        {QStringLiteral("theme.light"),      QStringLiteral("浅色")},
        {QStringLiteral("theme.dark"),       QStringLiteral("深色")},
        {QStringLiteral("find"),             QStringLiteral("查找")},
        {QStringLiteral("find.prev"),        QStringLiteral("上一个")},
        {QStringLiteral("find.next"),        QStringLiteral("下一个")},
        {QStringLiteral("find.close"),       QStringLiteral("关闭")},
        {QStringLiteral("menu.language"),    QStringLiteral("语言")},
        {QStringLiteral("lang.zh"),          QStringLiteral("中文")},
        {QStringLiteral("lang.en"),          QStringLiteral("English")},
    };

    // 英文
    m_en = {
        {QStringLiteral("app.title"),        QStringLiteral("Breeze")},
        {QStringLiteral("nav.back"),         QStringLiteral("Back")},
        {QStringLiteral("nav.forward"),      QStringLiteral("Forward")},
        {QStringLiteral("nav.reload"),       QStringLiteral("Reload")},
        {QStringLiteral("nav.stop"),         QStringLiteral("Stop")},
        {QStringLiteral("nav.home"),         QStringLiteral("Home")},
        {QStringLiteral("nav.newTab"),       QStringLiteral("New Tab")},
        {QStringLiteral("nav.newPrivate"),   QStringLiteral("New Private Tab")},
        {QStringLiteral("url.placeholder"),  QStringLiteral("Search or enter URL")},
        {QStringLiteral("bookmark.add"),     QStringLiteral("Add Bookmark")},
        {QStringLiteral("bookmark.manage"),  QStringLiteral("Manage Bookmarks")},
        {QStringLiteral("downloads"),        QStringLiteral("Downloads")},
        {QStringLiteral("history"),          QStringLiteral("History")},
        {QStringLiteral("settings"),         QStringLiteral("Settings")},
        {QStringLiteral("sync"),             QStringLiteral("Cloud Sync")},
        {QStringLiteral("userscript"),       QStringLiteral("User Scripts")},
        {QStringLiteral("theme"),            QStringLiteral("Theme")},
        {QStringLiteral("theme.system"),     QStringLiteral("Follow System")},
        {QStringLiteral("theme.light"),      QStringLiteral("Light")},
        {QStringLiteral("theme.dark"),       QStringLiteral("Dark")},
        {QStringLiteral("find"),             QStringLiteral("Find")},
        {QStringLiteral("find.prev"),        QStringLiteral("Previous")},
        {QStringLiteral("find.next"),        QStringLiteral("Next")},
        {QStringLiteral("find.close"),       QStringLiteral("Close")},
        {QStringLiteral("menu.language"),    QStringLiteral("Language")},
        {QStringLiteral("lang.zh"),          QStringLiteral("中文")},
        {QStringLiteral("lang.en"),          QStringLiteral("English")},
    };
}
