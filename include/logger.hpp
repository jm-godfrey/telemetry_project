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

    bool openLogFile(const std::string& filename);

    // Logs a telemetry data entry to the file, including a timestamp
    void logData(const TelemetryData& data);

    void closeLogFile();

private:

    std::ofstream logFile;
};