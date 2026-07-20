#include "gpuhwmon.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

bool pathExists(const std::string &path)
{
    return access(path.c_str(), F_OK) == 0;
}

bool hasNumericSuffix(const std::string &name, const std::string &prefix)
{
    if (name.compare(0, prefix.size(), prefix) != 0 || name.size() == prefix.size()) {
        return false;
    }

    for (size_t i = prefix.size(); i < name.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> childDirectories(const std::string &path)
{
    std::vector<std::string> directories;
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        return directories;
    }

    while (dirent *entry = readdir(dir)) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        const std::string childPath = path + "/" + entry->d_name;
        struct stat status {};
        if (stat(childPath.c_str(), &status) == 0 && S_ISDIR(status.st_mode)) {
            directories.push_back(childPath);
        }
    }

    closedir(dir);
    std::sort(directories.begin(), directories.end());
    return directories;
}

bool readIntFile(const std::string &path, long long *value)
{
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    char buffer[64] {};
    const ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytesRead <= 0) {
        return false;
    }

    char *end = nullptr;
    errno = 0;
    const long long parsed = std::strtoll(buffer, &end, 10);
    if (errno != 0 || end == buffer) {
        return false;
    }

    *value = parsed;
    return true;
}

std::string readTextFile(const std::string &path)
{
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return {};
    }

    char buffer[128] {};
    const ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytesRead <= 0) {
        return {};
    }

    std::string text(buffer, static_cast<size_t>(bytesRead));
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return text;
}

int temperatureInputIndex(const std::string &name)
{
    constexpr size_t prefixLength = 4;
    constexpr size_t suffixLength = 6;
    if (name.compare(0, prefixLength, "temp") != 0
        || name.size() <= prefixLength + suffixLength
        || name.compare(name.size() - suffixLength, suffixLength, "_input") != 0) {
        return -1;
    }

    const std::string indexText = name.substr(prefixLength, name.size() - prefixLength - suffixLength);
    if (indexText.empty()
        || !std::all_of(indexText.begin(), indexText.end(), [](unsigned char character) { return std::isdigit(character); })) {
        return -1;
    }

    return std::atoi(indexText.c_str());
}

int temperatureInputScore(const std::string &hwmonPath, const std::string &name)
{
    const int index = temperatureInputIndex(name);
    int score = index;
    const std::string label = readTextFile(hwmonPath + "/" + name.substr(0, name.size() - 6) + "_label");
    if (label.find("edge") != std::string::npos || label.find("gpu") != std::string::npos) {
        score -= 40;
    } else if (label.find("junction") != std::string::npos || label.find("hotspot") != std::string::npos) {
        score -= 20;
    }
    return score;
}

GpuHwmonSensor bestTemperatureSensor(const std::string &hwmonPath)
{
    GpuHwmonSensor bestSensor;
    int bestScore = 0;
    DIR *dir = opendir(hwmonPath.c_str());
    if (!dir) {
        return bestSensor;
    }

    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (temperatureInputIndex(name) < 0) {
            continue;
        }

        const std::string inputPath = hwmonPath + "/" + name;
        long long temperatureMilliC = 0;
        if (!readIntFile(inputPath, &temperatureMilliC)
            || temperatureMilliC < 0
            || temperatureMilliC > 130000) {
            continue;
        }

        const int score = temperatureInputScore(hwmonPath, name);
        if (!bestSensor.isValid() || score < bestScore) {
            bestSensor.hwmonPath = hwmonPath;
            bestSensor.temperatureInputPath = inputPath;
            bestScore = score;
        }
    }

    closedir(dir);
    return bestSensor;
}

} // namespace

bool GpuHwmonSensor::isValid() const
{
    return !hwmonPath.empty() && !temperatureInputPath.empty();
}

GpuHwmonSensor discoverGpuHwmonSensor(const std::string &drmRoot, const bool requireFanControl)
{
    GpuHwmonSensor fallbackSensor;

    for (const std::string &cardPath : childDirectories(drmRoot)) {
        const std::string cardName = cardPath.substr(cardPath.find_last_of('/') + 1);
        if (!hasNumericSuffix(cardName, "card")) {
            continue;
        }

        for (const std::string &hwmonPath : childDirectories(cardPath + "/device/hwmon")) {
            const std::string hwmonName = hwmonPath.substr(hwmonPath.find_last_of('/') + 1);
            if (!hasNumericSuffix(hwmonName, "hwmon")) {
                continue;
            }

            const GpuHwmonSensor sensor = bestTemperatureSensor(hwmonPath);
            if (!sensor.isValid()) {
                continue;
            }

            const bool hasFanControl = pathExists(hwmonPath + "/pwm1_enable") && pathExists(hwmonPath + "/pwm1");
            if (hasFanControl) {
                return sensor;
            }
            if (!requireFanControl && !fallbackSensor.isValid()) {
                fallbackSensor = sensor;
            }
        }
    }

    return fallbackSensor;
}
