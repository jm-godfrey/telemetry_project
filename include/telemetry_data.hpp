#pragma once

#include <chrono>
#include <cstdint>

// few simple structs to hold the currently supported telemetry data types, to be extended as needed
struct GPSData
{
    double latitude = 0.0;
    double longitude = 0.0;
    double speed = 0.0;
    double heading = 0.0; // course over ground, degrees true (0 = North), from RMC
    bool validFix = false;
    uint64_t timeMs = 0; // UTC epoch ms parsed from the RMC sentence (0 = none)
    std::chrono::steady_clock::time_point rxTime{}; // when the Pi received this fix
};

struct AccelerometerData
{
    float accelX = 0.0f;
    float accelY = 0.0f;
    float accelZ = 0.0f;
    float gyroX = 0.0f; // degrees per second
    float gyroY = 0.0f;
    float gyroZ = 0.0f;
};

// Running total of IMU samples between logged rows
struct ImuAverage
{
    AccelerometerData sum;
    AccelerometerData last;
    int samples = 0;

    void add(const AccelerometerData& sample)
    {
        sum.accelX += sample.accelX;
        sum.accelY += sample.accelY;
        sum.accelZ += sample.accelZ;
        sum.gyroX += sample.gyroX;
        sum.gyroY += sample.gyroY;
        sum.gyroZ += sample.gyroZ;
        samples++;
    }

    // Returns the mean and starts a new total. Repeats the last mean if there were no samples.
    AccelerometerData take()
    {
        if (samples > 0)
        {
            last.accelX = sum.accelX / samples;
            last.accelY = sum.accelY / samples;
            last.accelZ = sum.accelZ / samples;
            last.gyroX = sum.gyroX / samples;
            last.gyroY = sum.gyroY / samples;
            last.gyroZ = sum.gyroZ / samples;
        }

        sum = AccelerometerData{};
        samples = 0;
        return last;
    }
};

struct TelemetryData
{
    GPSData gps;
    AccelerometerData accelerometer;
    ImuAverage imuAverage;

    uint64_t timestampMs = 0; // Unix timestamp in milliseconds
    uint64_t tMs = 0; // ms since the first logged row
    uint64_t fixAgeMs = 0; // ms since the GPS fix on this row arrived
};
