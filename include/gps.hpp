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

    // Gets the latest GPS data, returns a GPSData struct with the current values.
    // If no valid fix is available, validFix will be false.
    GPSData readData();

    void close();

private:

    int fd;

    bool sendUBX(const std::vector<uint8_t>& msg);

    bool readLine(std::string& line);

    bool parseNMEA(const std::string& line, GPSData& gpsData);

    bool configureGPS();

};