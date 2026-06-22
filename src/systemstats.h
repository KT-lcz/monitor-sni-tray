#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QString>
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

struct CpuMemNetworkSnapshot
{
    quint64 rxBytes = 0;
    bool valid = false;
};

struct CpuMemStats
{
    double cpuPercent = 0.0;
    CpuMemMemorySnapshot memory;
    struct CpuInfo
    {
        QString name;
        int performanceCoreCount = 0;
        int efficiencyCoreCount = 0;
        double performanceAverageMHz = 0.0;
    } cpu;
    struct NetworkStats
    {
        double downloadBytesPerSecond = 0.0;
        bool valid = false;
    } network;
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
    static CpuMemNetworkSnapshot readNetworkSnapshot();
    static CpuMemStats::CpuInfo readCpuInfo();
    static CpuMemStats::PackageStats parseTurbostatOutput(const QByteArray &output);

private:
    static QString readCpuName();
    CpuMemStats::PackageStats readPackageStats();
    void ensureTurbostatProcess();
    void consumeTurbostatOutput(const QByteArray &output);

    CpuMemCpuSnapshot m_lastCpu;
    bool m_hasLastCpu = false;
    CpuMemNetworkSnapshot m_lastNetwork;
    bool m_hasLastNetwork = false;
    QElapsedTimer m_networkTimer;
    QProcess m_turbostatProcess;
    QElapsedTimer m_turbostatRestartTimer;
    QElapsedTimer m_lastPackageStatsTimer;
    QByteArray m_turbostatBuffer;
    CpuMemStats::PackageStats m_lastPackageStats;
};

#endif // SYSTEMSTATS_H
