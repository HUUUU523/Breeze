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

// ---- 崩溃恢复标志 ----
// 启动时调用：写入运行标志；若上次标志仍存在（异常退出）则返回 true
bool checkAndMarkRunning();
// 正常退出时调用：清除运行标志
void clearRunningFlag();

} // namespace Logger

#endif // LOGGER_H
