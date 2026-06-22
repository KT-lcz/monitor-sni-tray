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

    qInfo() << "CPU:" << stats.cpuPercent
            << "MEM:" << stats.memory.usedPercent()
            << "USED_KB:" << stats.memory.usedKb()
            << "TOTAL_KB:" << stats.memory.totalKb;
    return 0;
}
