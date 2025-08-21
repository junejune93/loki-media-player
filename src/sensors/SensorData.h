#pragma once

#include <chrono>
#include <string>

struct SensorData {
    std::chrono::steady_clock::time_point timestamp;

    double temperature = 0.0;
    double humidity = 0.0;
    double acceleration = 0.0;

    std::string source = "CSV";
};