#include "../src/systemstats.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    CpuMemStatsReader reader;
    QThread::msleep(120);
    const CpuMemStats stats = reader.sample();

    if (stats.memory.totalKb == 0) {
        qCritical() << "failed to read memory information";
        return 1;
    }

    if (stats.memory.availableKb > stats.memory.totalKb) {
        qCritical() << "memory available exceeds total";
        return 1;
    }

    if (stats.cpuPercent < 0.0 || stats.cpuPercent > 100.0) {
        qCritical() << "cpu percent out of range" << stats.cpuPercent;
        return 1;
    }

    if (stats.cpu.performanceCoreCount < 0 || stats.cpu.efficiencyCoreCount < 0) {
        qCritical() << "core count out of range"
                    << stats.cpu.performanceCoreCount
                    << stats.cpu.efficiencyCoreCount;
        return 1;
    }

    if (stats.cpu.performanceAverageMHz < 0.0) {
        qCritical() << "performance average frequency out of range" << stats.cpu.performanceAverageMHz;
        return 1;
    }

    if (stats.network.valid && stats.network.downloadBytesPerSecond < 0.0) {
        qCritical() << "download speed out of range" << stats.network.downloadBytesPerSecond;
        return 1;
    }

    for (const CpuMemDiskSnapshot &disk : stats.disks.volumes) {
        if (disk.name.isEmpty()) {
            qCritical() << "disk name is empty";
            return 1;
        }

        if (disk.totalBytes == 0 || disk.availableBytes > disk.totalBytes) {
            qCritical() << "disk capacity out of range"
                        << disk.name
                        << disk.availableBytes
                        << disk.totalBytes;
            return 1;
        }

        const double usedPercent = disk.usedPercent();
        if (usedPercent < 0.0 || usedPercent > 100.0) {
            qCritical() << "disk percent out of range" << disk.name << usedPercent;
            return 1;
        }
    }

    if (stats.disks.valid()) {
        const double disksPercent = stats.disks.usedPercent();
        if (disksPercent < 0.0 || disksPercent > 100.0) {
            qCritical() << "aggregate disk percent out of range" << disksPercent;
            return 1;
        }
    }

    const CpuMemStats::PackageStats packageStats = CpuMemStatsReader::parseTurbostatOutput(
        "PkgTmp\tPkgWatt\n"
        "41\t7.15\n"
        "PkgTmp\tPkgWatt\n"
        "43\t8.25\n");
    if (!packageStats.valid || packageStats.temperatureC != 43.0 || packageStats.powerW != 8.25) {
        qCritical() << "failed to parse turbostat package stats"
                    << packageStats.valid
                    << packageStats.temperatureC
                    << packageStats.powerW;
        return 1;
    }

    const CpuMemStats::PackageStats invalidPackageStats = CpuMemStatsReader::parseTurbostatOutput("PkgTmp PkgWatt\n");
    if (invalidPackageStats.valid) {
        qCritical() << "invalid turbostat output parsed as valid";
        return 1;
    }

    qInfo() << "CPU:" << stats.cpuPercent
            << "MEM:" << stats.memory.usedPercent()
            << "USED_KB:" << stats.memory.usedKb()
            << "TOTAL_KB:" << stats.memory.totalKb
            << "CPU_NAME:" << stats.cpu.name
            << "P_CORES:" << stats.cpu.performanceCoreCount
            << "E_CORES:" << stats.cpu.efficiencyCoreCount
            << "P_AVG_MHZ:" << stats.cpu.performanceAverageMHz
            << "NET_VALID:" << stats.network.valid
            << "DL_BPS:" << stats.network.downloadBytesPerSecond
            << "DISKS:" << stats.disks.volumes.size()
            << "DISK_USED_PERCENT:" << stats.disks.usedPercent()
            << "PKG_VALID:" << stats.package.valid
            << "PKG_TMP:" << stats.package.temperatureC
            << "PKG_WATT:" << stats.package.powerW;
    return 0;
}
