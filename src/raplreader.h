#ifndef RAPLREADER_H
#define RAPLREADER_H

#include <cstdint>
#include <string>
#include <vector>

struct RaplEnergyZone
{
    std::string energyPath;
    std::uint64_t maxEnergyRangeUj = 0;
    std::uint64_t previousEnergyUj = 0;
};

std::vector<RaplEnergyZone> discoverPackageRaplZones(
    const std::string &powercapRoot = "/sys/class/powercap");
bool readRaplEnergyUj(const std::string &path, std::uint64_t *energyUj);
bool sampleRaplPower(std::vector<RaplEnergyZone> *zones, double elapsedSeconds, double *powerW);

#endif // RAPLREADER_H
