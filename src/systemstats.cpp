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

CpuMemStatsReader::~CpuMemStatsReader()
{
    if (m_turbostatProcess.state() == QProcess::NotRunning) {
        return;
    }

    m_turbostatProcess.terminate();
    if (!m_turbostatProcess.waitForFinished(100)) {
        m_turbostatProcess.kill();
        m_turbostatProcess.waitForFinished(100);
    }
}

CpuMemStats CpuMemStatsReader::sample()
{
    CpuMemStats stats;
    stats.memory = readMemorySnapshot();
    stats.package = readPackageStats();

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

CpuMemStats::PackageStats CpuMemStatsReader::parseTurbostatOutput(const QByteArray &output)
{
    CpuMemStats::PackageStats stats;
    const QList<QByteArray> lines = output.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.simplified();
        if (line.isEmpty()) {
            continue;
        }

        const QList<QByteArray> values = line.split(' ');
        if (values.size() != 2) {
            continue;
        }

        bool tempOk = false;
        bool powerOk = false;
        const double temperature = values.at(0).toDouble(&tempOk);
        const double power = values.at(1).toDouble(&powerOk);
        if (!tempOk || !powerOk || power < 0.0) {
            continue;
        }

        stats.temperatureC = temperature;
        stats.powerW = power;
        stats.valid = true;
    }

    return stats;
}

CpuMemStats::PackageStats CpuMemStatsReader::readPackageStats()
{
    ensureTurbostatProcess();

    if (m_turbostatProcess.state() == QProcess::Running) {
        consumeTurbostatOutput(m_turbostatProcess.readAllStandardOutput());
    }

    return m_lastPackageStats;
}

void CpuMemStatsReader::ensureTurbostatProcess()
{
    if (m_turbostatProcess.state() != QProcess::NotRunning) {
        return;
    }

    m_lastPackageStats = {};
    m_lastPackageStatsTimer.invalidate();

    if (m_turbostatRestartTimer.isValid() && m_turbostatRestartTimer.elapsed() < 10000) {
        return;
    }

    m_turbostatBuffer.clear();
    m_turbostatRestartTimer.restart();
    m_turbostatProcess.start(QStringLiteral("turbostat"),
                             {
                                 QStringLiteral("--quiet"),
                                 QStringLiteral("--show"),
                                 QStringLiteral("PkgTmp,PkgWatt"),
                                 QStringLiteral("-i"),
                                 QStringLiteral("1"),
                             });
}

void CpuMemStatsReader::consumeTurbostatOutput(const QByteArray &output)
{
    if (output.isEmpty()) {
        return;
    }

    m_turbostatBuffer.append(output);

    const int lastNewline = m_turbostatBuffer.lastIndexOf('\n');
    if (lastNewline < 0) {
        return;
    }

    const QByteArray completeOutput = m_turbostatBuffer.left(lastNewline + 1);
    m_turbostatBuffer = m_turbostatBuffer.mid(lastNewline + 1);

    const CpuMemStats::PackageStats stats = parseTurbostatOutput(completeOutput);
    if (stats.valid) {
        m_lastPackageStats = stats;
        m_lastPackageStatsTimer.restart();
    } else if (m_lastPackageStatsTimer.isValid() && m_lastPackageStatsTimer.elapsed() > 5000) {
        m_lastPackageStats = {};
        m_lastPackageStatsTimer.invalidate();
    }
}
