#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

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
};

class CpuMemStatsReader
{
public:
    CpuMemStatsReader();

    CpuMemStats sample();
    static CpuMemCpuSnapshot readCpuSnapshot();
    static CpuMemMemorySnapshot readMemorySnapshot();

private:
    CpuMemCpuSnapshot m_lastCpu;
    bool m_hasLastCpu = false;
};

#endif // SYSTEMSTATS_H
