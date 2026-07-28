#include "gpuhwmon.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
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

bool readIntFile(const std::string &path, int *value)
{
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    char buffer[32] {};
    const ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
    const int savedErrno = errno;
    close(fd);
    if (bytesRead <= 0) {
        errno = savedErrno;
        return false;
    }

    char *end = nullptr;
    errno = 0;
    const long parsed = std::strtol(buffer, &end, 10);
    if (errno != 0 || end == buffer || parsed <= 0 || parsed > 65535) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool writeManualSpeed(const std::string &enablePath, const std::string &pwmPath,
                      int maxPwm, int percent)
{
    const int pwm = (maxPwm * percent + 50) / 100;
    const std::string pwmValue = std::to_string(pwm) + "\n";
    if (!writeFile(enablePath, "1\n")) {
        return false;
    }
    if (writeFile(pwmPath, pwmValue.c_str())) {
        return true;
    }

    writeFile(enablePath, "0\n");
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "usage: monitor-gpu-fan-helper auto|low|high|full\n";
        return 2;
    }

    const std::string mode = argv[1];
    if (mode != "auto" && mode != "low" && mode != "high" && mode != "full") {
        std::cerr << "invalid mode: " << mode << '\n';
        return 2;
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

    int maxPwm = 255;
    readIntFile(sensor.hwmonPath + "/pwm1_max", &maxPwm);
    if (mode == "low") {
        return writeManualSpeed(pwmEnablePath, pwmPath, maxPwm, 50) ? 0 : 1;
    }
    if (mode == "high") {
        return writeManualSpeed(pwmEnablePath, pwmPath, maxPwm, 80) ? 0 : 1;
    }
    return writeManualSpeed(pwmEnablePath, pwmPath, maxPwm, 100) ? 0 : 1;
}
