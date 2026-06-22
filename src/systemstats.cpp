#include "systemstats.h"

#include <QByteArray>
#include <QFile>

quint64 CpuMemMemorySnapshot::usedKb() const
{
    if (totalKb <= availableKb) {
        return 0;
    }

    return totalKb - availableKb;
}

double CpuMemMemorySnapshot::usedPercent() const
{
    if (totalKb == 0) {
        return 0.0;
    }

    return usedKb() * 100.0 / totalKb;
}

CpuMemStatsReader::CpuMemStatsReader()
    : m_lastCpu(readCpuSnapshot())
    , m_hasLastCpu(m_lastCpu.total > 0)
{
}

CpuMemStats CpuMemStatsReader::sample()
{
    CpuMemStats stats;
    stats.memory = readMemorySnapshot();

    const CpuMemCpuSnapshot currentCpu = readCpuSnapshot();
    if (m_hasLastCpu && currentCpu.total > m_lastCpu.total) {
        const quint64 totalDelta = currentCpu.total - m_lastCpu.total;
        const quint64 idleDelta = currentCpu.idle >= m_lastCpu.idle ? currentCpu.idle - m_lastCpu.idle : 0;

        if (totalDelta > 0 && totalDelta >= idleDelta) {
            stats.cpuPercent = (totalDelta - idleDelta) * 100.0 / totalDelta;
        }
    }

    if (currentCpu.total > 0) {
        m_lastCpu = currentCpu;
        m_hasLastCpu = true;
    }

    return stats;
}

CpuMemCpuSnapshot CpuMemStatsReader::readCpuSnapshot()
{
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QList<QByteArray> values = file.readLine().simplified().split(' ');
    if (values.size() < 6 || values.first() != QByteArrayLiteral("cpu")) {
        return {};
    }

    CpuMemCpuSnapshot snapshot;
    for (int i = 1; i < values.size(); ++i) {
        bool ok = false;
        const quint64 value = values.at(i).toULongLong(&ok);
        if (ok) {
            snapshot.total += value;
        }
    }

    const quint64 idle = values.at(4).toULongLong();
    const quint64 ioWait = values.at(5).toULongLong();
    snapshot.idle = idle + ioWait;

    return snapshot;
}

CpuMemMemorySnapshot CpuMemStatsReader::readMemorySnapshot()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QList<QByteArray> lines = file.readAll().split('\n');

    CpuMemMemorySnapshot snapshot;
    for (const QByteArray &line : lines) {
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() < 2) {
            continue;
        }

        bool ok = false;
        const quint64 value = parts.at(1).toULongLong(&ok);
        if (!ok) {
            continue;
        }

        if (parts.at(0) == QByteArrayLiteral("MemTotal:")) {
            snapshot.totalKb = value;
        } else if (parts.at(0) == QByteArrayLiteral("MemAvailable:")) {
            snapshot.availableKb = value;
        }

        if (snapshot.totalKb > 0 && snapshot.availableKb > 0) {
            break;
        }
    }

    return snapshot;
}
