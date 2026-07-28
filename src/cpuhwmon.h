#ifndef CPUHWMON_H
#define CPUHWMON_H

#include <string>

struct CpuHwmonSensor
{
    std::string temperatureInputPath;

    bool isValid() const;
};

CpuHwmonSensor discoverCpuHwmonSensor(const std::string &hwmonRoot = "/sys/class/hwmon");

#endif // CPUHWMON_H
