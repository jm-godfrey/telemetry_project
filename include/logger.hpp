#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include "telemetry_data.hpp"

class Logger
{
public:

    Logger();
    ~Logger();

    bool openLogFile();

    // Logs a telemetry data entry to the file, including a timestamp
    void logData(const TelemetryData& data);

    void closeLogFile();

private:

    std::string generateFilename();

    std::ofstream logFile;
    std::mutex logMutex;
};