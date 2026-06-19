#include "../include/logger.hpp"

#include <iomanip>
#include <sstream>
#include <ctime>
#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

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

    // creates full fd because need it for fdatasync()
    fd = ::open(fullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        std::cerr << "[Logger] Failed to open log file '" << fullPath
                  << "': " << std::strerror(errno) << "\n";
        return false;
    }

    std::cout << "[Logger] Logging to: " << fullPath << "\n";

    // CSV header
    const char* header = "timestamp,lat,lon,speed,accelX,accelY,accelZ\n";
    ::write(fd, header, std::strlen(header));

    lastSync = std::chrono::steady_clock::now();

    return true;
}

void Logger::closeLogFile() {
    if (fd >= 0)
    {
        ::fdatasync(fd);
        ::close(fd);
        fd = -1;
    }
}

void Logger::logData(const TelemetryData& data) {
    if (fd < 0) return;

    std::lock_guard<std::mutex> lock(logMutex);

    std::ostringstream row;
    row
        << data.timestampMs << ","
        << data.gps.latitude << ","
        << data.gps.longitude << ","
        << data.gps.speed << ","
        << data.accelerometer.accelX << ","
        << data.accelerometer.accelY << ","
        << data.accelerometer.accelZ << "\n";

    const std::string line = row.str();
    if (::write(fd, line.data(), line.size()) < 0)
        std::cerr << "[Logger] Write failed: " << std::strerror(errno) << "\n";

    // forces buffered data onto the SD card with fdatasync()
    const auto now = std::chrono::steady_clock::now();
    if (now - lastSync >= std::chrono::seconds(1))
    {
        ::fdatasync(fd);
        lastSync = now;
    }
}
