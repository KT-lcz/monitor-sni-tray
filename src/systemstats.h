#ifndef SYSTEMSTATS_H
#define SYSTEMSTATS_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QMetaType>
#include <QVector>
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

struct CpuMemDiskSnapshot
{
    QString name;
    quint64 totalBytes = 0;
    quint64 availableBytes = 0;

    quint64 usedBytes() const;
    double usedPercent() const;
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
        bool temperatureValid = false;
        bool powerValid = false;

        bool valid() const;
    } package;
    struct GpuStats
    {
        double temperatureC = 0.0;
        int fanPwm = 0;
        int fanMaxPwm = 255;
        int fanPercent = 0;
        int fanRpm = 0;
        bool automaticFan = false;
        bool valid = false;
        bool fanValid = false;
        bool rpmValid = false;

        QString fanLevel() const;
    } gpu;
    struct DiskStats
    {
        QVector<CpuMemDiskSnapshot> volumes;

        bool valid() const;
        quint64 totalBytes() const;
        quint64 usedBytes() const;
        double usedPercent() const;
    } disks;
};

Q_DECLARE_METATYPE(CpuMemStats)

class CpuMemStatsReader
{
public:
    CpuMemStatsReader();
    ~CpuMemStatsReader();

    CpuMemStats sample();
    static CpuMemCpuSnapshot readCpuSnapshot();
    static CpuMemMemorySnapshot readMemorySnapshot();
    static CpuMemNetworkSnapshot readNetworkSnapshot();
    static QVector<CpuMemDiskSnapshot> readDiskSnapshots();
    static CpuMemStats::CpuInfo readCpuInfo();
    static CpuMemStats::PackageStats parsePowerHelperOutput(const QByteArray &output);

private:
    static QString readCpuName();
    CpuMemStats::GpuStats readGpuStats();
    void refreshGpuHwmonSensor();
    void refreshCpuHwmonSensor();
    CpuMemStats::PackageStats readPackageStats();
    void ensurePowerHelperProcess();
    void consumePowerHelperOutput(const QByteArray &output);

    CpuMemCpuSnapshot m_lastCpu;
    bool m_hasLastCpu = false;
    CpuMemNetworkSnapshot m_lastNetwork;
    bool m_hasLastNetwork = false;
    QElapsedTimer m_networkTimer;
    QString m_gpuHwmonPath;
    QString m_gpuTemperatureInputPath;
    QElapsedTimer m_gpuDiscoveryTimer;
    QString m_cpuTemperatureInputPath;
    QElapsedTimer m_cpuDiscoveryTimer;
    QProcess m_powerHelperProcess;
    QElapsedTimer m_powerHelperRestartTimer;
    QElapsedTimer m_lastPowerStatsTimer;
    QByteArray m_powerHelperBuffer;
    CpuMemStats::PackageStats m_lastPackageStats;
};

#endif // SYSTEMSTATS_H
