#pragma once

#include "telemetry_data.hpp"

class Accelerometer
{
public:
    Accelerometer();
    ~Accelerometer();

    // Sets up I2C communication with the accelerometer module, returns true if successful
    bool initialise();

    // Gets the latest accelerometer data, returns an AccelerometerData struct with the current values.
    AccelerometerData readData();

    void close();

private:

    int fd;

    // used to set up things like the sample rate
    bool writeRegister(uint8_t reg, uint8_t value);

    uint8_t readRegister(uint8_t reg);

    // used to read two bytes low byte and high byte for each axis, combines them into a single signed 16-bit value
    int16_t readAxis(uint8_t lowReg);
};