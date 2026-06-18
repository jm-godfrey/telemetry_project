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
#include <csignal>

using namespace std;

// Shutdown flag, shared with the worker threads and written by the signal
// handler. File-scope so the handler can reach it. atomic<bool> is one of the
// few things a signal handler is allowed to touch safely.
static std::atomic<bool> running(true);

// SIGINT (Ctrl-C) / SIGTERM handler: only flips the flag, nothing else.
static void handleSignal(int)
{
    running = false;
}

int main()
{
    cout << "Hello, Telemetry!" << endl;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

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

    // Keep main alive until a shutdown signal flips `running`.
    while (running)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    cout << "\nShutting down, stopping threads...\n";

    // Threads check `running` at the top of each loop and exit; join() waits
    // for them to actually finish their current iteration.
    imuThread.join();
    gpsThread.join();
    loggerThread.join();

    logger.closeLogFile();

    cout << "Shutdown complete.\n";

    return 0;
}
