#pragma once

#include "telemetry_data.hpp"
#include <string>
#include <vector>

class GPS
{
public:
    GPS();
    ~GPS();

    // Sets up I2C communication with the GPS module, returns true if successful
    bool initialise();

    // Reads every line waiting on the module and returns the newest valid fix.
    // validFix is false if this pass found no valid fix.
    GPSData readData();

    void close();

private:

    int fd;

    // Line being received, kept between reads as lines can arrive in pieces
    std::string lineBuf;

    bool sendUBX(const std::vector<uint8_t>& msg);

    bool readLine(std::string& line);

    bool parseNMEA(const std::string& line, GPSData& gpsData);

    bool configureGPS();

    // Throws away everything queued on the module so logging never starts on old data
    void flushBacklog();

};