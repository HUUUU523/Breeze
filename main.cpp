#include "browserwindow.h"
#include "logger.h"
#include "settingsdialog.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QStandardPaths>
#include <QWebEngineProfile>

int main(int argc, char *argv[])
{
    // Qt6 WebEngine：Chromium 要求先构造 QApplication 再做相关设置
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Breeze"));
    QApplication::setApplicationVersion(QStringLiteral(BREEZE_VERSION));
    QApplication::setOrganizationName(QStringLiteral("Breeze"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/B.ico")));

    // 安装文件日志（AppData/logs/breeze.log）
    Logger::install();

    // 应用全局代理（来自设置）
    SettingsDialog::applyProxy();

    // 使用持久化 Profile，缓存/存储到本地（默认 profile 已是持久化，这里显式命名）
    // 数据写入用户可写的 AppData 目录，而非程序目录（Program Files 下无写权限）
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    profile->setPersistentStoragePath(dataDir + QStringLiteral("/profile"));
    profile->setCachePath(dataDir + QStringLiteral("/profile/cache"));
    profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);

    BrowserWindow window;
    window.show();

    return app.exec();
}
