#include "trayapplication.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLockFile>
#include <QStandardPaths>

#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(QStringLiteral("monitor-sni-tray"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("monitor-sni-tray"));

    QString runtimeDirectory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtimeDirectory.isEmpty()) {
        runtimeDirectory = QDir::tempPath() + QStringLiteral("/monitor-sni-tray-%1").arg(getuid());
        QDir().mkpath(runtimeDirectory);
    }
    QLockFile instanceLock(runtimeDirectory + QStringLiteral("/monitor-sni-tray.lock"));
    instanceLock.setStaleLockTime(30000);
    if (!instanceLock.tryLock(100)) {
        return 0;
    }

    MonitorTrayApplication tray;
    if (!tray.isAvailable()) {
        return 1;
    }

    return application.exec();
}
