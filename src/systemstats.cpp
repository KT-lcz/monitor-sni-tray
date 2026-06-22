#include "systemstats.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QStorageInfo>
#include <QStringList>
#include <QVector>
#include <algorithm>

namespace {

QString rootBlockDeviceName(QString deviceName)
{
    if (deviceName.startsWith(QStringLiteral("/dev/"))) {
        deviceName = QFileInfo(deviceName).fileName();
    }

    QString current = deviceName;
    while (!current.isEmpty()) {
        QFile partitionFile(QStringLiteral("/sys/class/block/%1/partition").arg(current));
        if (!partitionFile.exists()) {
            return QFileInfo::exists(QStringLiteral("/sys/class/block/%1").arg(current)) ? current : QString();
        }

        const QFileInfo parentInfo(QStringLiteral("/sys/class/block/%1/..").arg(current));
        const QString parent = parentInfo.canonicalFilePath().section('/', -1);
        if (parent.isEmpty() || parent == current) {
            return {};
        }

        current = parent;
    }

    return {};
}

} // namespace

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

quint64 CpuMemDiskSnapshot::usedBytes() const
{
    if (totalBytes <= availableBytes) {
        return 0;
    }

    return totalBytes - availableBytes;
}

double CpuMemDiskSnapshot::usedPercent() const
{
    if (totalBytes == 0) {
        return 0.0;
    }

    return usedBytes() * 100.0 / totalBytes;
}

bool CpuMemStats::DiskStats::valid() const
{
    return !volumes.isEmpty();
}

quint64 CpuMemStats::DiskStats::totalBytes() const
{
    quint64 total = 0;
    for (const CpuMemDiskSnapshot &volume : volumes) {
        total += volume.totalBytes;
    }

    return total;
}

quint64 CpuMemStats::DiskStats::usedBytes() const
{
    quint64 used = 0;
    for (const CpuMemDiskSnapshot &volume : volumes) {
        used += volume.usedBytes();
    }

    return used;
}

double CpuMemStats::DiskStats::usedPercent() const
{
    const quint64 total = totalBytes();
    if (total == 0) {
        return 0.0;
    }

    return usedBytes() * 100.0 / total;
}

CpuMemStatsReader::CpuMemStatsReader()
    : m_lastCpu(readCpuSnapshot())
    , m_hasLastCpu(m_lastCpu.total > 0)
    , m_lastNetwork(readNetworkSnapshot())
    , m_hasLastNetwork(m_lastNetwork.valid)
{
    m_networkTimer.start();
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
    stats.cpu = readCpuInfo();
    stats.package = readPackageStats();
    stats.disks.volumes = readDiskSnapshots();

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

    const CpuMemNetworkSnapshot currentNetwork = readNetworkSnapshot();
    const qint64 networkElapsed = m_networkTimer.isValid() ? m_networkTimer.elapsed() : 0;
    if (currentNetwork.valid) {
        stats.network.valid = true;
        if (m_hasLastNetwork && currentNetwork.rxBytes >= m_lastNetwork.rxBytes && networkElapsed > 0) {
            stats.network.downloadBytesPerSecond = (currentNetwork.rxBytes - m_lastNetwork.rxBytes) * 1000.0 / networkElapsed;
        }
        m_lastNetwork = currentNetwork;
        m_hasLastNetwork = true;
        m_networkTimer.restart();
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

CpuMemNetworkSnapshot CpuMemStatsReader::readNetworkSnapshot()
{
    QFile file(QStringLiteral("/proc/net/dev"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    CpuMemNetworkSnapshot snapshot;
    while (true) {
        const QByteArray line = file.readLine();
        if (line.isNull()) {
            break;
        }

        const int colon = line.indexOf(':');
        if (colon < 0) {
            continue;
        }

        const QString interfaceName = QString::fromLatin1(line.left(colon).trimmed());
        if (interfaceName == QStringLiteral("lo")) {
            continue;
        }

        const QList<QByteArray> values = line.mid(colon + 1).simplified().split(' ');
        if (values.size() < 1) {
            continue;
        }

        bool ok = false;
        const quint64 rxBytes = values.at(0).toULongLong(&ok);
        if (ok) {
            snapshot.rxBytes += rxBytes;
            snapshot.valid = true;
        }
    }

    return snapshot;
}

QVector<CpuMemDiskSnapshot> CpuMemStatsReader::readDiskSnapshots()
{
    QHash<QString, CpuMemDiskSnapshot> disksByName;
    QSet<QString> seenFileSystems;

    const QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &volume : volumes) {
        if (!volume.isValid() || !volume.isReady() || volume.bytesTotal() <= 0 || volume.bytesAvailable() < 0) {
            continue;
        }

        const QString device = QString::fromLocal8Bit(volume.device());
        if (!device.startsWith(QStringLiteral("/dev/")) || device.startsWith(QStringLiteral("/dev/loop"))) {
            continue;
        }

        if (seenFileSystems.contains(device)) {
            continue;
        }

        const QString diskName = rootBlockDeviceName(device);
        if (diskName.isEmpty()) {
            continue;
        }

        CpuMemDiskSnapshot &disk = disksByName[diskName];
        disk.name = diskName;
        disk.totalBytes += static_cast<quint64>(volume.bytesTotal());
        disk.availableBytes += static_cast<quint64>(volume.bytesAvailable());
        seenFileSystems.insert(device);
    }

    QVector<CpuMemDiskSnapshot> disks = disksByName.values().toVector();
    std::sort(disks.begin(), disks.end(), [](const CpuMemDiskSnapshot &left, const CpuMemDiskSnapshot &right) {
        return left.name < right.name;
    });

    return disks;
}

CpuMemStats::CpuInfo CpuMemStatsReader::readCpuInfo()
{
    CpuMemStats::CpuInfo info;
    static const QString cpuName = readCpuName();
    info.name = cpuName;

    struct PolicyInfo
    {
        int cpu = -1;
        int physicalCore = -1;
        quint64 maxKHz = 0;
        quint64 currentKHz = 0;
    };

    QVector<PolicyInfo> policies;
    const QDir cpufreqDir(QStringLiteral("/sys/devices/system/cpu/cpufreq"));
    const QFileInfoList policyDirs = cpufreqDir.entryInfoList({ QStringLiteral("policy*") }, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &policyDir : policyDirs) {
        const QString policyName = policyDir.fileName();
        bool cpuOk = false;
        const int cpu = policyName.mid(QStringLiteral("policy").size()).toInt(&cpuOk);
        if (!cpuOk) {
            continue;
        }

        auto readUIntFile = [](const QString &path) -> quint64 {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                return 0;
            }

            bool ok = false;
            const quint64 value = file.readAll().trimmed().toULongLong(&ok);
            return ok ? value : 0;
        };

        quint64 currentKHz = readUIntFile(policyDir.filePath() + QStringLiteral("/scaling_cur_freq"));
        if (currentKHz == 0) {
            currentKHz = readUIntFile(policyDir.filePath() + QStringLiteral("/cpuinfo_avg_freq"));
        }

        const quint64 maxKHz = readUIntFile(policyDir.filePath() + QStringLiteral("/cpuinfo_max_freq"));
        if (maxKHz == 0 || currentKHz == 0) {
            continue;
        }

        int physicalCore = cpu;
        int packageId = 0;
        QFile packageFile(QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/physical_package_id").arg(cpu));
        if (packageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            bool packageOk = false;
            const int parsedPackageId = packageFile.readAll().trimmed().toInt(&packageOk);
            if (packageOk) {
                packageId = parsedPackageId;
            }
        }

        QFile topologyFile(QStringLiteral("/sys/devices/system/cpu/cpu%1/topology/core_id").arg(cpu));
        if (topologyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            bool coreOk = false;
            const int coreId = topologyFile.readAll().trimmed().toInt(&coreOk);
            if (coreOk) {
                physicalCore = packageId * 100000 + coreId;
            }
        }

        policies.append({ cpu, physicalCore, maxKHz, currentKHz });
    }

    if (policies.isEmpty()) {
        return info;
    }

    QVector<quint64> maxFrequencies;
    for (const PolicyInfo &policy : policies) {
        if (!maxFrequencies.contains(policy.maxKHz)) {
            maxFrequencies.append(policy.maxKHz);
        }
    }

    std::sort(maxFrequencies.begin(), maxFrequencies.end());
    quint64 performanceMinKHz = maxFrequencies.isEmpty() ? 0 : maxFrequencies.last();
    if (maxFrequencies.size() > 1) {
        quint64 largestGap = 0;
        int splitIndex = maxFrequencies.size() - 1;
        for (int i = 1; i < maxFrequencies.size(); ++i) {
            const quint64 gap = maxFrequencies.at(i) - maxFrequencies.at(i - 1);
            if (gap > largestGap) {
                largestGap = gap;
                splitIndex = i;
            }
        }

        if (largestGap * 10 >= maxFrequencies.last()) {
            performanceMinKHz = maxFrequencies.at(splitIndex);
        } else {
            performanceMinKHz = maxFrequencies.first();
        }
    }

    QSet<int> performanceCores;
    QSet<int> efficiencyCores;
    QHash<int, double> performanceCoreTotalKHz;
    QHash<int, int> performanceCoreThreadCount;

    for (const PolicyInfo &policy : policies) {
        if (policy.maxKHz >= performanceMinKHz) {
            performanceCores.insert(policy.physicalCore);
            performanceCoreTotalKHz[policy.physicalCore] += policy.currentKHz;
            performanceCoreThreadCount[policy.physicalCore] += 1;
        } else {
            efficiencyCores.insert(policy.physicalCore);
        }
    }

    info.performanceCoreCount = performanceCores.size();
    info.efficiencyCoreCount = efficiencyCores.size();
    if (!performanceCores.isEmpty()) {
        double performanceCoreAverageTotalKHz = 0.0;
        for (const int coreId : performanceCores) {
            performanceCoreAverageTotalKHz += performanceCoreTotalKHz.value(coreId) / performanceCoreThreadCount.value(coreId, 1);
        }
        info.performanceAverageMHz = performanceCoreAverageTotalKHz / performanceCores.size() / 1000.0;
    }

    return info;
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

QString CpuMemStatsReader::readCpuName()
{
    QFile file(QStringLiteral("/proc/cpuinfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QString fallbackName;
    while (true) {
        const QByteArray line = file.readLine();
        if (line.isNull()) {
            break;
        }

        const int colon = line.indexOf(':');
        if (colon < 0) {
            continue;
        }

        const QByteArray key = line.left(colon).trimmed().toLower();
        if (key == QByteArrayLiteral("model name")
            || key == QByteArrayLiteral("hardware")
            || key == QByteArrayLiteral("cpu model")) {
            const QString value = QString::fromUtf8(line.mid(colon + 1).trimmed());
            if (!value.isEmpty()) {
                return value;
            }
        } else if (key == QByteArrayLiteral("processor")) {
            bool numericProcessor = false;
            const QString value = QString::fromUtf8(line.mid(colon + 1).trimmed());
            value.toInt(&numericProcessor);
            if (!numericProcessor && !value.isEmpty() && fallbackName.isEmpty()) {
                fallbackName = value;
            }
        }
    }

    if (!fallbackName.isEmpty()) {
        return fallbackName;
    }

    QProcess lscpu;
    lscpu.start(QStringLiteral("lscpu"), { QStringLiteral("--json") });
    if (lscpu.waitForFinished(500)) {
        const QList<QByteArray> lines = lscpu.readAllStandardOutput().split('\n');
        for (const QByteArray &line : lines) {
            if (!line.contains("\"Model name:\"")) {
                continue;
            }

            const int dataIndex = line.indexOf("\"data\"");
            const int colon = dataIndex >= 0 ? line.indexOf(':', dataIndex) : -1;
            if (colon < 0) {
                continue;
            }

            QString value = QString::fromUtf8(line.mid(colon + 1).trimmed());
            value.remove('"');
            value.remove(',');
            value = value.trimmed();
            if (!value.isEmpty()) {
                return value;
            }
        }
    }

    return {};
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
