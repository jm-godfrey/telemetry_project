#pragma once

#include "telemetry_data.hpp"

class Accelerometer
{
public:
    Accelerometer();
    ~Accelerometer();

    // Sets up I2C communication with the accelerometer module, returns true if successful
    bool initialise();

    // Reads the latest gyro and accelerometer values together, returns false if the read failed.
    bool readData(AccelerometerData& data);

    void close();

private:

    int fd;

    // used to set up things like the sample rate
    bool writeRegister(uint8_t reg, uint8_t value);

    uint8_t readRegister(uint8_t reg);
};