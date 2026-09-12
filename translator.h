#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QHash>
#include <QObject>
#include <QSettings>
#include <QString>

// 极简多语言：代码内字典，支持中文/英文切换
class Translator : public QObject
{
    Q_OBJECT

public:
    static Translator *instance();

    QString language() const;          // "zh" / "en"
    void setLanguage(const QString &lang);

    // 取翻译；无对应则返回 key 本身
    QString t(const QString &key) const;

signals:
    void languageChanged();

private:
    Translator();
    void buildDictionaries();

    QString m_lang;
    QHash<QString, QString> m_zh;
    QHash<QString, QString> m_en;
};

// 便捷宏
inline QString TR(const QString &key) {
    return Translator::instance()->t(key);
}

#endif // TRANSLATOR_H
