#pragma once

#include "telemetry_data.hpp"
#include <string>

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

    bool readLine(std::string& line);

    bool parseNMEA(const std::string& line, GPSData& gpsData);

};