#ifndef LOGGER_H
#define LOGGER_H

#include <QString>

// 简易文件日志：安装 qInstallMessageHandler，把 qDebug/qWarning/qCritical
// 等输出追加到 AppData/logs/breeze.log，便于排查线上问题。
namespace Logger {

// 安装消息处理器，日志写入 AppData/logs/breeze.log
void install();

// 当前日志文件完整路径
QString logFilePath();

} // namespace Logger

#endif // LOGGER_H
