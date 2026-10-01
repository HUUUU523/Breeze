#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include <QString>
#include <QStringList>

// 本地多用户配置管理：每个用户独立的书签/历史/设置/会话目录。
// 数据根目录：<AppData>/Breeze/profiles/<用户名>/
// 当前用户记录在 <AppData>/Breeze/profiles/current.txt
namespace ProfileManager {

// 所有用户（目录名）列表
QStringList profiles();

// 当前用户名（默认 "默认"）
QString currentProfile();

// 切换当前用户
void setCurrentProfile(const QString &name);

// 新建用户（返回是否成功）
bool createProfile(const QString &name);

// 删除用户（及其数据）
bool deleteProfile(const QString &name);

// 当前用户的数据目录（会自动创建）
QString dataDir();

// 当前用户的 QSettings 标识（org/app 用不同 app 名实现隔离）
QString settingsAppName();

} // namespace ProfileManager

#endif // PROFILEMANAGER_H
