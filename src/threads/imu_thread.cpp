#include "../../include/threads/imu_thread.hpp"

void imuThreadFunc(Accelerometer& accel,
                   TelemetryData& sharedData,
                   std::mutex& dataMutex,
                   std::atomic<bool>& running)
{
    while (running)
    {
        auto start = std::chrono::steady_clock::now();

        AccelerometerData accelData = accel.readData();

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            sharedData.accelerometer = accelData;
        }

        std::this_thread::sleep_until(start + std::chrono::milliseconds(10)); // 100 Hz
    }
}