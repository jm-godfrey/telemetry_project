#include "../include/gps.hpp"
#include "../include/accelerometer.hpp"
#include "../include/logger.hpp"
#include "../include/telemetry_data.hpp"
#include "../include/threads/gps_thread.hpp"
#include "../include/threads/imu_thread.hpp"
#include "../include/threads/logger_thread.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

using namespace std;

int main()
{
    cout << "Hello, Telemetry!" << endl;

    GPS gps;
    Accelerometer accel;
    Logger logger;

    if (!gps.initialise() || !accel.initialise())
    {
        cerr << "Failed to initialise sensors\n";
        return 1;
    }

    if (!logger.openLogFile())
    {
        cerr << "Failed to open log file\n";
        return 1;
    }

    TelemetryData sharedData;
    std::mutex dataMutex;
    std::atomic<bool> running(true);

    // Create threads
    std::thread imuThread(imuThreadFunc,
                          std::ref(accel),
                          std::ref(sharedData),
                          std::ref(dataMutex),
                          std::ref(running));

    std::thread gpsThread(gpsThreadFunc,
                          std::ref(gps),
                          std::ref(sharedData),
                          std::ref(dataMutex),
                          std::ref(running));

    std::thread loggerThread(loggerThreadFunc,
                             std::ref(logger),
                             std::ref(sharedData),
                             std::ref(dataMutex),
                             std::ref(running));

    // Keep main alive
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    running = false;

    imuThread.join();
    gpsThread.join();
    loggerThread.join();

    logger.closeLogFile();

    return 0;
}
