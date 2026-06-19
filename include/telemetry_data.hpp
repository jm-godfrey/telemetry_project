#pragma once

#include <cstdint>

// few simple structs to hold the currently supported telemetry data types, to be extended as needed
struct GPSData
{
    double latitude = 0.0;
    double longitude = 0.0;
    double speed = 0.0;
    bool validFix = false;
    uint64_t timeMs = 0; // UTC epoch ms parsed from the RMC sentence (0 = none)
};

struct AccelerometerData
{
    float accelX = 0.0f;
    float accelY = 0.0f;
    float accelZ = 0.0f;
};

struct TelemetryData
{
    GPSData gps;
    AccelerometerData accelerometer;

    uint64_t timestampMs = 0; // Unix timestamp in milliseconds
};
