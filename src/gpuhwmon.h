#ifndef GPUHWMON_H
#define GPUHWMON_H

#include <string>

struct GpuHwmonSensor
{
    std::string hwmonPath;
    std::string temperatureInputPath;

    bool isValid() const;
};

GpuHwmonSensor discoverGpuHwmonSensor(const std::string &drmRoot = "/sys/class/drm",
                                      bool requireFanControl = false);

#endif // GPUHWMON_H
