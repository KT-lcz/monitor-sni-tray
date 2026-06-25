#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace {

bool pathExists(const std::string &path)
{
    return access(path.c_str(), F_OK) == 0;
}

bool isNumericCardName(const std::string &name)
{
    constexpr const char *prefix = "card";
    constexpr size_t prefixLength = 4;
    if (name.compare(0, prefixLength, prefix) != 0 || name.size() == prefixLength) {
        return false;
    }

    for (size_t i = prefixLength; i < name.size(); ++i) {
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
        struct stat st {};
        if (stat(childPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            directories.push_back(childPath);
        }
    }

    closedir(dir);
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

bool hasPlausibleTemperatureInput(const std::string &hwmonPath)
{
    DIR *dir = opendir(hwmonPath.c_str());
    if (!dir) {
        return false;
    }

    bool found = false;
    while (dirent *entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name.compare(0, 4, "temp") != 0 || name.size() <= 10 || name.rfind("_input") != name.size() - 6) {
            continue;
        }

        long long milliC = 0;
        if (readIntFile(hwmonPath + "/" + name, &milliC) && milliC >= 0 && milliC <= 130000) {
            found = true;
            break;
        }
    }

    closedir(dir);
    return found;
}

std::string findGpuHwmonPath()
{
    for (const std::string &cardPath : childDirectories("/sys/class/drm")) {
        const std::string cardName = cardPath.substr(cardPath.find_last_of('/') + 1);
        if (!isNumericCardName(cardName)) {
            continue;
        }

        for (const std::string &hwmonPath : childDirectories(cardPath + "/device/hwmon")) {
            if (pathExists(hwmonPath + "/pwm1_enable")
                && pathExists(hwmonPath + "/pwm1")
                && hasPlausibleTemperatureInput(hwmonPath)) {
                return hwmonPath;
            }
        }
    }

    return {};
}

bool writeFile(const char *path, const char *value)
{
    const int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        std::cerr << "failed to open " << path << ": " << std::strerror(errno) << '\n';
        return false;
    }

    const size_t length = std::strlen(value);
    const ssize_t written = write(fd, value, length);
    const int savedErrno = errno;
    close(fd);

    if (written != static_cast<ssize_t>(length)) {
        std::cerr << "failed to write " << path << ": " << std::strerror(savedErrno) << '\n';
        return false;
    }

    return true;
}

bool writeFile(const std::string &path, const char *value)
{
    return writeFile(path.c_str(), value);
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "usage: gpu-fan-helper auto|medium|full\n";
        return 2;
    }

    const std::string mode = argv[1];
    if (mode != "auto" && mode != "medium" && mode != "full") {
        std::cerr << "invalid mode: " << mode << '\n';
        return 2;
    }

    if (setuid(0) != 0) {
        std::cerr << "failed to switch to root: " << std::strerror(errno) << '\n';
        return 1;
    }

    const std::string hwmonPath = findGpuHwmonPath();
    if (hwmonPath.empty()) {
        std::cerr << "failed to find GPU hwmon fan control path\n";
        return 1;
    }

    const std::string pwmEnablePath = hwmonPath + "/pwm1_enable";
    const std::string pwmPath = hwmonPath + "/pwm1";

    if (mode == "auto") {
        return writeFile(pwmEnablePath, "0\n") ? 0 : 1;
    }

    if (mode == "medium") {
        return writeFile(pwmEnablePath, "1\n") && writeFile(pwmPath, "204\n") ? 0 : 1;
    }

    if (mode == "full") {
        return writeFile(pwmEnablePath, "1\n") && writeFile(pwmPath, "255\n") ? 0 : 1;
    }

    return 2;
}
