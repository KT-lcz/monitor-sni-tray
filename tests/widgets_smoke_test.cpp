#include "../src/quickpanelwidget.h"
#include "../src/tipswidget.h"
#include "../src/monitorwidget.h"

#include <QApplication>
#include <QDebug>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QtGlobal>

static QImage renderWidget(QWidget &widget)
{
    widget.resize(widget.sizeHint().expandedTo(widget.size()));
    QPixmap pixmap(widget.size());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    widget.render(&painter);
    return pixmap.toImage();
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }

    QApplication app(argc, argv);

    CpuMemStats stats;
    stats.cpuPercent = 12.3;
    stats.cpu.name = QStringLiteral("Test CPU With A Long Name");
    stats.cpu.performanceCoreCount = 4;
    stats.cpu.efficiencyCoreCount = 8;
    stats.cpu.performanceAverageMHz = 3456.0;
    stats.memory.totalKb = 32ULL * 1024ULL * 1024ULL;
    stats.memory.availableKb = 12ULL * 1024ULL * 1024ULL;
    stats.network.valid = true;
    stats.network.downloadBytesPerSecond = 2.5 * 1024.0 * 1024.0;
    stats.package.valid = true;
    stats.package.temperatureC = 47.0;
    stats.package.powerW = 12.5;
    stats.gpu.valid = true;
    stats.gpu.temperatureC = 52.0;
    stats.gpu.fanValid = true;
    stats.gpu.fanPwm = 160;
    stats.gpu.fanMaxPwm = 255;
    stats.gpu.fanPercent = 63;

    stats.disks.volumes.append({
        QStringLiteral("nvme0n1"),
        512ULL * 1024ULL * 1024ULL * 1024ULL,
        128ULL * 1024ULL * 1024ULL * 1024ULL,
    });
    stats.disks.volumes.append({
        QStringLiteral("nvme1n1"),
        2ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL,
        750ULL * 1024ULL * 1024ULL * 1024ULL,
    });

    CpuMemTipsWidget tips;
    tips.setStats(stats);
    renderWidget(tips);

    CpuMemQuickPanelWidget quickPanel;
    quickPanel.setStats(stats);
    renderWidget(quickPanel);

    CpuMemMonitorWidget monitor;
    monitor.setStats(stats);
    renderWidget(monitor);

    CpuMemStats coolerGpuStats = stats;
    coolerGpuStats.gpu.temperatureC = 40.0;
    CpuMemMonitorWidget coolerGpuMonitor;
    coolerGpuMonitor.setStats(coolerGpuStats);
    const QImage coolerGpuImage = renderWidget(coolerGpuMonitor);

    CpuMemStats hotterGpuStats = stats;
    hotterGpuStats.gpu.temperatureC = 90.0;
    CpuMemMonitorWidget hotterGpuMonitor;
    hotterGpuMonitor.setStats(hotterGpuStats);
    const QImage hotterGpuImage = renderWidget(hotterGpuMonitor);

    const QColor coolerGpuColor = coolerGpuImage.pixelColor(coolerGpuImage.rect().center());
    const QColor hotterGpuColor = hotterGpuImage.pixelColor(hotterGpuImage.rect().center());
    if (coolerGpuColor != hotterGpuColor) {
        qCritical() << "monitor icon color changed with GPU temperature"
                    << coolerGpuColor
                    << hotterGpuColor;
        return 1;
    }

    return 0;
}
