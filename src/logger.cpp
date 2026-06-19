#include "../include/logger.hpp"

#include <iomanip>
#include <sstream>
#include <ctime>
#include <iostream>
#include <filesystem>

Logger::Logger() {}

Logger::~Logger() { closeLogFile(); }

std::string Logger::generateFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm* tm = std::localtime(&time);

    std::stringstream ss;
    ss << "telemetry_log_"
       << (tm->tm_year + 1900)
       << std::setw(2) << std::setfill('0') << (tm->tm_mon + 1)
       << std::setw(2) << std::setfill('0') << tm->tm_mday << "_"
       << std::setw(2) << std::setfill('0') << tm->tm_hour
       << std::setw(2) << std::setfill('0') << tm->tm_min
       << std::setw(2) << std::setfill('0') << tm->tm_sec
       << ".csv";

    return ss.str();
}

bool Logger::openLogFile() {
    std::string filename = generateFilename();

    const std::string logDir = "/home/jmgodfrey/Documents/Vscode/telemetry_project/data/logs/";

    // Create the log directory if it doesn't exist yet
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    if (ec)
    {
        std::cerr << "[Logger] Failed to create log directory '" << logDir
                  << "': " << ec.message() << "\n";
        return false;
    }

    std::string fullPath = logDir + filename;
    logFile.open(fullPath);

    if (!logFile.is_open())
        return false;
    
    std::cout << "[Logger] Logging to: " << fullPath << "\n";

    // CSV header
    logFile << "timestamp,lat,lon,speed,accelX,accelY,accelZ\n";

    return true;
}

void Logger::closeLogFile() {
    if (logFile.is_open())
        logFile.close();
}

void Logger::logData(const TelemetryData& data) {
    if (!logFile.is_open()) return;

    std::lock_guard<std::mutex> lock(logMutex);

    logFile 
        << data.timestampMs << ","
        << data.gps.latitude << ","
        << data.gps.longitude << ","
        << data.gps.speed << ","
        << data.accelerometer.accelX << ","
        << data.accelerometer.accelY << ","
        << data.accelerometer.accelZ << "\n";
}
