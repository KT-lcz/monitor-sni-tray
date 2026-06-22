#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QProcess>
#include <QtGlobal>

struct CpuMemCpuSnapshot
{
    quint64 total = 0;
    quint64 idle = 0;
};

struct CpuMemMemorySnapshot
{
    quint64 totalKb = 0;
    quint64 availableKb = 0;

    quint64 usedKb() const;
    double usedPercent() const;
};

struct CpuMemStats
{
    double cpuPercent = 0.0;
    CpuMemMemorySnapshot memory;
    struct PackageStats
    {
        double temperatureC = 0.0;
        double powerW = 0.0;
        bool valid = false;
    } package;
};

class CpuMemStatsReader
{
public:
    CpuMemStatsReader();
    ~CpuMemStatsReader();

    CpuMemStats sample();
    static CpuMemCpuSnapshot readCpuSnapshot();
    static CpuMemMemorySnapshot readMemorySnapshot();
    static CpuMemStats::PackageStats parseTurbostatOutput(const QByteArray &output);

private:
    CpuMemStats::PackageStats readPackageStats();
    void ensureTurbostatProcess();
    void consumeTurbostatOutput(const QByteArray &output);

    CpuMemCpuSnapshot m_lastCpu;
    bool m_hasLastCpu = false;
    QProcess m_turbostatProcess;
    QElapsedTimer m_turbostatRestartTimer;
    QElapsedTimer m_lastPackageStatsTimer;
    QByteArray m_turbostatBuffer;
    CpuMemStats::PackageStats m_lastPackageStats;
};

#endif // SYSTEMSTATS_H
