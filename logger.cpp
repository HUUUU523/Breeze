#include "logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QStandardPaths>
#include <QTextStream>
#include <cstdio>

namespace {

QMutex g_mutex;
QString g_logPath;

QString resolveLogPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/logs");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/breeze.log");
}

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "DEBUG";
    case QtInfoMsg:     return "INFO ";
    case QtWarningMsg:  return "WARN ";
    case QtCriticalMsg: return "CRIT ";
    case QtFatalMsg:    return "FATAL";
    }
    return "?????";
}

void messageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const QString line = QStringLiteral("[%1] [%2] %3\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
             QString::fromLatin1(levelName(type)),
             msg);

    // 控制台也输出（开发时可见）
    std::fputs(line.toLocal8Bit().constData(), stderr);
    std::fflush(stderr);

    QMutexLocker locker(&g_mutex);
    QFile f(g_logPath);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(line.toUtf8());
        f.close();
    }

    if (type == QtFatalMsg)
        abort();
}

} // namespace

namespace Logger {

void install()
{
    g_logPath = resolveLogPath();
    qInstallMessageHandler(messageHandler);
    qInfo() << "Breeze" << BREEZE_VERSION << "启动，日志：" << g_logPath;
}

QString logFilePath()
{
    if (g_logPath.isEmpty())
        g_logPath = resolveLogPath();
    return g_logPath;
}

} // namespace Logger
