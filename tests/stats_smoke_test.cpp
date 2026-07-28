#include "../src/systemstats.h"
#include "../src/cpuhwmon.h"
#include "../src/gpuhwmon.h"
#include "../src/raplreader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

namespace {

bool writeTestFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}

bool testGpuHwmonDiscovery()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        qCritical() << "failed to create temporary GPU discovery directory";
        return false;
    }

    const QString fallbackPath = temporaryDirectory.path() + QStringLiteral("/card0/device/hwmon/hwmon2");
    const QString controlledPath = temporaryDirectory.path() + QStringLiteral("/card1/device/hwmon/hwmon7");
    const QString connectorPath = temporaryDirectory.path() + QStringLiteral("/card1-DP-1/device/hwmon/hwmon8");
    if (!QDir().mkpath(fallbackPath)
        || !QDir().mkpath(controlledPath)
        || !QDir().mkpath(connectorPath)
        || !writeTestFile(fallbackPath + QStringLiteral("/temp1_input"), "48000\n")
        || !writeTestFile(controlledPath + QStringLiteral("/temp1_input"), "72000\n")
        || !writeTestFile(controlledPath + QStringLiteral("/temp1_label"), "junction\n")
        || !writeTestFile(controlledPath + QStringLiteral("/temp2_input"), "61000\n")
        || !writeTestFile(controlledPath + QStringLiteral("/temp2_label"), "edge\n")
        || !writeTestFile(controlledPath + QStringLiteral("/pwm1"), "128\n")
        || !writeTestFile(controlledPath + QStringLiteral("/pwm1_enable"), "0\n")
        || !writeTestFile(connectorPath + QStringLiteral("/temp1_input"), "42000\n")
        || !writeTestFile(connectorPath + QStringLiteral("/pwm1"), "255\n")
        || !writeTestFile(connectorPath + QStringLiteral("/pwm1_enable"), "1\n")) {
        qCritical() << "failed to prepare temporary GPU discovery tree";
        return false;
    }

    const GpuHwmonSensor controlledSensor = discoverGpuHwmonSensor(temporaryDirectory.path().toStdString(), true);
    if (controlledSensor.hwmonPath != controlledPath.toStdString()
        || controlledSensor.temperatureInputPath != (controlledPath + QStringLiteral("/temp2_input")).toStdString()) {
        qCritical() << "failed to prefer the controllable GPU edge sensor"
                    << QString::fromStdString(controlledSensor.hwmonPath)
                    << QString::fromStdString(controlledSensor.temperatureInputPath);
        return false;
    }

    if (!QFile::remove(controlledPath + QStringLiteral("/pwm1"))
        || !QFile::remove(controlledPath + QStringLiteral("/pwm1_enable"))) {
        qCritical() << "failed to remove temporary fan control files";
        return false;
    }

    const GpuHwmonSensor fallbackSensor = discoverGpuHwmonSensor(temporaryDirectory.path().toStdString());
    if (fallbackSensor.hwmonPath != fallbackPath.toStdString()) {
        qCritical() << "failed to fall back to a temperature-only GPU sensor"
                    << QString::fromStdString(fallbackSensor.hwmonPath);
        return false;
    }

    if (discoverGpuHwmonSensor(temporaryDirectory.path().toStdString(), true).isValid()) {
        qCritical() << "fan-control discovery accepted a temperature-only sensor";
        return false;
    }

    return true;
}

bool testCpuHwmonDiscovery()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        qCritical() << "failed to create temporary CPU discovery directory";
        return false;
    }

    const QString coretempPath = temporaryDirectory.path() + QStringLiteral("/hwmon2");
    const QString unrelatedPath = temporaryDirectory.path() + QStringLiteral("/hwmon3");
    if (!QDir().mkpath(coretempPath)
        || !QDir().mkpath(unrelatedPath)
        || !writeTestFile(coretempPath + QStringLiteral("/name"), "coretemp\n")
        || !writeTestFile(coretempPath + QStringLiteral("/temp1_input"), "47000\n")
        || !writeTestFile(coretempPath + QStringLiteral("/temp1_label"), "Package id 0\n")
        || !writeTestFile(coretempPath + QStringLiteral("/temp2_input"), "42000\n")
        || !writeTestFile(coretempPath + QStringLiteral("/temp2_label"), "Core 0\n")
        || !writeTestFile(unrelatedPath + QStringLiteral("/name"), "nvme\n")
        || !writeTestFile(unrelatedPath + QStringLiteral("/temp1_input"), "60000\n")) {
        qCritical() << "failed to prepare temporary CPU discovery tree";
        return false;
    }

    const CpuHwmonSensor sensor = discoverCpuHwmonSensor(temporaryDirectory.path().toStdString());
    if (sensor.temperatureInputPath != (coretempPath + QStringLiteral("/temp1_input")).toStdString()) {
        qCritical() << "failed to select CPU package temperature"
                    << QString::fromStdString(sensor.temperatureInputPath);
        return false;
    }
    return true;
}

bool testRaplReader()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        qCritical() << "failed to create temporary RAPL directory";
        return false;
    }

    const QString packagePath = temporaryDirectory.path() + QStringLiteral("/intel-rapl:0");
    const QString corePath = temporaryDirectory.path() + QStringLiteral("/intel-rapl:0:0");
    if (!QDir().mkpath(packagePath)
        || !QDir().mkpath(corePath)
        || !writeTestFile(packagePath + QStringLiteral("/name"), "package-0\n")
        || !writeTestFile(packagePath + QStringLiteral("/energy_uj"), "9000000\n")
        || !writeTestFile(packagePath + QStringLiteral("/max_energy_range_uj"), "10000000\n")
        || !writeTestFile(corePath + QStringLiteral("/name"), "core\n")
        || !writeTestFile(corePath + QStringLiteral("/energy_uj"), "4000000\n")
        || !writeTestFile(corePath + QStringLiteral("/max_energy_range_uj"), "10000000\n")) {
        qCritical() << "failed to prepare temporary RAPL tree";
        return false;
    }

    std::vector<RaplEnergyZone> zones = discoverPackageRaplZones(temporaryDirectory.path().toStdString());
    if (zones.size() != 1
        || !writeTestFile(packagePath + QStringLiteral("/energy_uj"), "1000000\n")) {
        qCritical() << "failed to discover package RAPL zone" << zones.size();
        return false;
    }

    double powerW = 0.0;
    if (!sampleRaplPower(&zones, 0.5, &powerW) || powerW != 4.0) {
        qCritical() << "failed to calculate wrapped RAPL power" << powerW;
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (!testGpuHwmonDiscovery()) {
        return 1;
    }
    if (!testCpuHwmonDiscovery() || !testRaplReader()) {
        return 1;
    }

    CpuMemStats::GpuStats fanModeStats;
    fanModeStats.fanValid = true;
    fanModeStats.automaticFan = true;
    if (fanModeStats.fanLevel() != QStringLiteral("自动")) {
        qCritical() << "failed to label automatic GPU fan mode";
        return 1;
    }
    fanModeStats.automaticFan = false;
    fanModeStats.fanPercent = 50;
    if (fanModeStats.fanLevel() != QStringLiteral("低速")) {
        qCritical() << "failed to label low GPU fan mode";
        return 1;
    }
    fanModeStats.fanPercent = 80;
    if (fanModeStats.fanLevel() != QStringLiteral("高速")) {
        qCritical() << "failed to label high GPU fan mode";
        return 1;
    }
    fanModeStats.fanPercent = 100;
    if (fanModeStats.fanLevel() != QStringLiteral("满速")) {
        qCritical() << "failed to label full GPU fan mode";
        return 1;
    }

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

    if (stats.gpu.valid) {
        if (stats.gpu.temperatureC < 0.0 || stats.gpu.temperatureC > 130.0) {
            qCritical() << "gpu temperature out of range" << stats.gpu.temperatureC;
            return 1;
        }

        if (stats.gpu.fanValid
            && (stats.gpu.fanPwm < 0
                || stats.gpu.fanMaxPwm <= 0
                || stats.gpu.fanPwm > stats.gpu.fanMaxPwm
                || stats.gpu.fanPercent < 0
                || stats.gpu.fanPercent > 100)) {
            qCritical() << "gpu fan pwm out of range"
                        << stats.gpu.fanPwm
                        << stats.gpu.fanMaxPwm
                        << stats.gpu.fanPercent;
            return 1;
        }
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

    const CpuMemStats::PackageStats packageStats = CpuMemStatsReader::parsePowerHelperOutput(
        "7.15\n8.25\n");
    if (!packageStats.powerValid || packageStats.temperatureValid || packageStats.powerW != 8.25) {
        qCritical() << "failed to parse power helper stats"
                    << packageStats.powerValid
                    << packageStats.powerW;
        return 1;
    }

    const CpuMemStats::PackageStats invalidPackageStats = CpuMemStatsReader::parsePowerHelperOutput(
        "invalid\nnan\n10001\n");
    if (invalidPackageStats.valid()) {
        qCritical() << "invalid power helper output parsed as valid";
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
            << "PKG_VALID:" << stats.package.valid()
            << "PKG_TMP_VALID:" << stats.package.temperatureValid
            << "PKG_WATT_VALID:" << stats.package.powerValid
            << "PKG_TMP:" << stats.package.temperatureC
            << "PKG_WATT:" << stats.package.powerW
            << "GPU_VALID:" << stats.gpu.valid
            << "GPU_TMP:" << stats.gpu.temperatureC
            << "GPU_FAN:" << stats.gpu.fanLevel()
            << "GPU_FAN_PERCENT:" << stats.gpu.fanPercent;
    return 0;
}
