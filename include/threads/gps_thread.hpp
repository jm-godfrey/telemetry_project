#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include "../gps.hpp"
#include "../telemetry_data.hpp"

void gpsThreadFunc(GPS& gps, TelemetryData& sharedData, std::mutex& dataMutex, std::atomic<bool>& running);