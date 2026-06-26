#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

constexpr const char *kPwmEnablePath = "/sys/class/drm/card0/device/hwmon/hwmon4/pwm1_enable";
constexpr const char *kPwmPath = "/sys/class/drm/card0/device/hwmon/hwmon4/pwm1";

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

    if (mode == "auto") {
        return writeFile(kPwmEnablePath, "0\n") ? 0 : 1;
    }

    if (mode == "medium") {
        return writeFile(kPwmEnablePath, "1\n") && writeFile(kPwmPath, "204\n") ? 0 : 1;
    }

    if (mode == "full") {
        return writeFile(kPwmEnablePath, "1\n") && writeFile(kPwmPath, "255\n") ? 0 : 1;
    }

    return 2;
}
