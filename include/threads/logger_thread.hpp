#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include "../logger.hpp"
#include "../telemetry_data.hpp"

void loggerThreadFunc(Logger& logger,
                      TelemetryData& sharedData,
                      std::mutex& dataMutex,
                      std::atomic<bool>& running);