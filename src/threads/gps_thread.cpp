#include "../../include/threads/gps_thread.hpp"

void gpsThreadFunc(GPS& gps,
                   TelemetryData& sharedData,
                   std::mutex& dataMutex,
                   std::atomic<bool>& running)
{
    while (running)
    {
        auto start = std::chrono::steady_clock::now();

        GPSData data = gps.readData();

        if (data.validFix)
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            sharedData.gps = data;
        }

        std::this_thread::sleep_until(start + std::chrono::milliseconds(100)); // 10 Hz
    }
}