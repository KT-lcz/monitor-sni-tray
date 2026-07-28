#include "raplreader.h"

#include <dirent.h>
#include <fstream>
#include <string>

namespace {

std::string readTextFile(const std::string &path)
{
    std::ifstream file(path);
    std::string value;
    std::getline(file, value);
    return value;
}

bool readUnsignedFile(const std::string &path, std::uint64_t *value)
{
    std::ifstream file(path);
    std::uint64_t parsed = 0;
    if (!(file >> parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

} // namespace

std::vector<RaplEnergyZone> discoverPackageRaplZones(const std::string &powercapRoot)
{
    std::vector<RaplEnergyZone> zones;
    DIR *root = opendir(powercapRoot.c_str());
    if (!root) {
        return zones;
    }

    while (dirent *entry = readdir(root)) {
        const std::string directoryName = entry->d_name;
        if (directoryName == "." || directoryName == "..") {
            continue;
        }

        const std::string directoryPath = powercapRoot + "/" + directoryName;
        const std::string zoneName = readTextFile(directoryPath + "/name");
        if (zoneName.compare(0, 8, "package-") != 0) {
            continue;
        }

        RaplEnergyZone zone;
        zone.energyPath = directoryPath + "/energy_uj";
        if (!readUnsignedFile(directoryPath + "/max_energy_range_uj", &zone.maxEnergyRangeUj)
            || !readUnsignedFile(zone.energyPath, &zone.previousEnergyUj)
            || zone.maxEnergyRangeUj == 0) {
            continue;
        }
        zones.push_back(zone);
    }

    closedir(root);
    return zones;
}

bool readRaplEnergyUj(const std::string &path, std::uint64_t *energyUj)
{
    return readUnsignedFile(path, energyUj);
}

bool sampleRaplPower(std::vector<RaplEnergyZone> *zones, double elapsedSeconds, double *powerW)
{
    if (!zones || zones->empty() || !powerW || elapsedSeconds <= 0.0) {
        return false;
    }

    std::uint64_t totalDeltaUj = 0;
    for (RaplEnergyZone &zone : *zones) {
        std::uint64_t currentEnergyUj = 0;
        if (!readRaplEnergyUj(zone.energyPath, &currentEnergyUj)) {
            return false;
        }

        const std::uint64_t deltaUj = currentEnergyUj >= zone.previousEnergyUj
            ? currentEnergyUj - zone.previousEnergyUj
            : zone.maxEnergyRangeUj - zone.previousEnergyUj + currentEnergyUj;
        zone.previousEnergyUj = currentEnergyUj;
        totalDeltaUj += deltaUj;
    }

    *powerW = totalDeltaUj / (elapsedSeconds * 1000000.0);
    return true;
}
