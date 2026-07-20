#include "gpuhwmon.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

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

    const GpuHwmonSensor sensor = discoverGpuHwmonSensor("/sys/class/drm", true);
    if (!sensor.isValid()) {
        std::cerr << "failed to find GPU hwmon fan control path\n";
        return 1;
    }

    const std::string pwmEnablePath = sensor.hwmonPath + "/pwm1_enable";
    const std::string pwmPath = sensor.hwmonPath + "/pwm1";

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
