#include "raplreader.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

int main(int argc, char **)
{
    if (argc != 1) {
        std::cerr << "monitor-power-helper does not accept arguments\n";
        return 2;
    }

    std::vector<RaplEnergyZone> zones = discoverPackageRaplZones();
    if (zones.empty()) {
        std::cerr << "no readable package RAPL energy zones found\n";
        return 1;
    }

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    auto next = previous + std::chrono::seconds(1);
    while (true) {
        std::this_thread::sleep_until(next);
        const auto current = Clock::now();
        const double elapsedSeconds = std::chrono::duration<double>(current - previous).count();

        double powerW = 0.0;
        if (!sampleRaplPower(&zones, elapsedSeconds, &powerW)) {
            std::cerr << "failed to read package RAPL energy\n";
            return 1;
        }

        std::cout << std::fixed << std::setprecision(3) << powerW << '\n' << std::flush;
        previous = current;
        next += std::chrono::seconds(1);
    }
}
