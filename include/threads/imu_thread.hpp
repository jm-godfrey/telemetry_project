#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include "../accelerometer.hpp"
#include "../telemetry_data.hpp"

void imuThreadFunc(Accelerometer& accel,
                   TelemetryData& sharedData,
                   std::mutex& dataMutex,
                   std::atomic<bool>& running);