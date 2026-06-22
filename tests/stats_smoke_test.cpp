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
            << "PKG_VALID:" << stats.package.valid
            << "PKG_TMP:" << stats.package.temperatureC
            << "PKG_WATT:" << stats.package.powerW;
    return 0;
}
