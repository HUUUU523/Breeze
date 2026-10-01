#include "profilemanager.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

QString profilesRoot()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/profiles");
    QDir().mkpath(dir);
    return dir;
}

QString currentFilePath()
{
    return profilesRoot() + QStringLiteral("/current.txt");
}

} // namespace

namespace ProfileManager {

QStringList profiles()
{
    QStringList out;
    QDir root(profilesRoot());
    for (const QFileInfo &fi : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        out << fi.fileName();
    if (out.isEmpty())
        out << QStringLiteral("默认");
    return out;
}

QString currentProfile()
{
    QFile f(currentFilePath());
    if (f.open(QIODevice::ReadOnly)) {
        const QString name = QString::fromUtf8(f.readAll()).trimmed();
        if (!name.isEmpty())
            return name;
    }
    return QStringLiteral("默认");
}

void setCurrentProfile(const QString &name)
{
    if (name.isEmpty())
        return;
    QFile f(currentFilePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(name.toUtf8());
    // 确保目录存在
    QDir().mkpath(profilesRoot() + QLatin1Char('/') + name);
}

bool createProfile(const QString &name)
{
    if (name.isEmpty())
        return false;
    const QString dir = profilesRoot() + QLatin1Char('/') + name;
    if (QDir(dir).exists())
        return false;
    return QDir().mkpath(dir);
}

bool deleteProfile(const QString &name)
{
    if (name.isEmpty() || name == QStringLiteral("默认"))
        return false;   // 不允许删默认用户
    QDir dir(profilesRoot() + QLatin1Char('/') + name);
    return dir.removeRecursively();
}

QString dataDir()
{
    const QString dir = profilesRoot() + QLatin1Char('/') + currentProfile();
    QDir().mkpath(dir);
    return dir;
}

QString settingsAppName()
{
    // QSettings(org, app) 用 app 名区分不同用户
    return QStringLiteral("Breeze-") + currentProfile();
}

} // namespace ProfileManager
