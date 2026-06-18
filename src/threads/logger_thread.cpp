#include "../../include/threads/logger_thread.hpp"
#include <iostream>

void loggerThreadFunc(Logger& logger,
                      TelemetryData& sharedData,
                      std::mutex& dataMutex,
                      std::atomic<bool>& running)
{
    int count = 0;

    while (running)
    {
        auto start = std::chrono::steady_clock::now();

        TelemetryData copy;

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            copy = sharedData;
        }

        copy.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        logger.logData(copy);

        // Optional debug print
        std::cout << "Data point: " << count++ << "\n";
        std::cout << "Lat: " << copy.gps.latitude << " Lon: " << copy.gps.longitude << "\n";
        std::cout << "Speed: " << copy.gps.speed << "\n";
        std::cout << "Accel X: " << copy.accelerometer.accelX << "\n";
        std::cout << "-----------------------------\n";

        std::this_thread::sleep_until(start + std::chrono::milliseconds(100)); // 10 Hz
    }
}