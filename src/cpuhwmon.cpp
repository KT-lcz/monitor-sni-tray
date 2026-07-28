#include "cpuhwmon.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <limits>
#include <string>

namespace {

std::string readTextFile(const std::string &path)
{
    std::ifstream file(path);
    std::string value;
    std::getline(file, value);
    return value;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool isCpuTemperatureDriver(const std::string &name)
{
    return name == "coretemp"
        || name == "k10temp"
        || name == "zenpower"
        || name == "cpu_thermal"
        || name == "x86_pkg_temp";
}

int temperatureInputIndex(const std::string &name)
{
    if (name.size() <= 10 || name.compare(0, 4, "temp") != 0
        || name.compare(name.size() - 6, 6, "_input") != 0) {
        return -1;
    }

    const std::string number = name.substr(4, name.size() - 10);
    char *end = nullptr;
    const long index = std::strtol(number.c_str(), &end, 10);
    if (!end || *end != '\0' || index <= 0 || index > std::numeric_limits<int>::max()) {
        return -1;
    }
    return static_cast<int>(index);
}

bool readTemperature(const std::string &path, long long *temperatureMilliC)
{
    std::ifstream file(path);
    long long value = 0;
    if (!(file >> value) || value < 0 || value > 130000) {
        return false;
    }
    *temperatureMilliC = value;
    return true;
}

int labelScore(const std::string &label, int index)
{
    const std::string normalized = lower(label);
    if (normalized.find("package id 0") != std::string::npos) {
        return 100;
    }
    if (normalized == "tctl") {
        return 95;
    }
    if (normalized == "tdie") {
        return 90;
    }
    if (normalized.find("package") != std::string::npos) {
        return 85;
    }
    if (normalized.find("cpu") != std::string::npos) {
        return 80;
    }
    return index == 1 ? 20 : 10;
}

} // namespace

bool CpuHwmonSensor::isValid() const
{
    return !temperatureInputPath.empty();
}

CpuHwmonSensor discoverCpuHwmonSensor(const std::string &hwmonRoot)
{
    CpuHwmonSensor best;
    int bestScore = -1;

    DIR *root = opendir(hwmonRoot.c_str());
    if (!root) {
        return best;
    }

    while (dirent *entry = readdir(root)) {
        const std::string directoryName = entry->d_name;
        if (directoryName == "." || directoryName == "..") {
            continue;
        }

        const std::string directoryPath = hwmonRoot + "/" + directoryName;
        const std::string driverName = lower(readTextFile(directoryPath + "/name"));
        if (!isCpuTemperatureDriver(driverName)) {
            continue;
        }

        DIR *directory = opendir(directoryPath.c_str());
        if (!directory) {
            continue;
        }

        while (dirent *sensorEntry = readdir(directory)) {
            const std::string inputName = sensorEntry->d_name;
            const int index = temperatureInputIndex(inputName);
            if (index < 0) {
                continue;
            }

            const std::string inputPath = directoryPath + "/" + inputName;
            long long temperatureMilliC = 0;
            if (!readTemperature(inputPath, &temperatureMilliC)) {
                continue;
            }

            const std::string labelPath = directoryPath + "/temp" + std::to_string(index) + "_label";
            const int score = labelScore(readTextFile(labelPath), index);
            if (score > bestScore) {
                best.temperatureInputPath = inputPath;
                bestScore = score;
            }
        }
        closedir(directory);
    }

    closedir(root);
    return best;
}
