#include "sensors/CsvSensorSource.h"
#include <sstream>
#include <iostream>
#include <chrono>
#include <utility>

CsvSensorSource::CsvSensorSource(std::string filename)
        : _filename(std::move(filename)) {
    loadSensorData();
}

CsvSensorSource::~CsvSensorSource() { CsvSensorSource::stop();
}

void CsvSensorSource::start() {
    if (_running) {
        return;
    }
    _running = true;
    _startTime = std::chrono::steady_clock::now();
    _currentIndex = 0;
    _thread = std::thread(&CsvSensorSource::run, this);
}

void CsvSensorSource::stop() {
    _running = false;
    if (_thread.joinable()) {
        _thread.join();
    }
}

void CsvSensorSource::flush() {
    _queue.clear();
}

void CsvSensorSource::loadSensorData() {
    _file.open(_filename);
    if (!_file.is_open()) {
        std::cerr << "Failed to open CSV: " << _filename << std::endl;
        return;
    }
    std::cout << "Loading sensor data from: " << _filename << std::endl;

    std::string line;

    // Skip: Comment (#)
    std::getline(_file, line);
    if (!line.empty() && line[0] == '#') {
        line = line.substr(1);
    }

    while (std::getline(_file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;

        // Split
        while (std::getline(ss, token, ',')) {
            // Remove any whitespace
            token.erase(0, token.find_first_not_of(" \t\n\r\f\v"));
            token.erase(token.find_last_not_of(" \t\n\r\f\v") + 1);
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }

        if (tokens.size() >= 4) {
            try {
                const auto timestamp = std::stoull(tokens[0]);
                const auto temperature = std::stod(tokens[1]);
                const auto humidity = std::stod(tokens[2]);
                const auto acceleration = std::stod(tokens[3]);

                _sensorData.push_back({
                                              std::chrono::steady_clock::time_point() +
                                              std::chrono::milliseconds(timestamp),
                                              temperature,
                                              humidity,
                                              acceleration,
                                              "CSV"
                                      });
            } catch (const std::exception &e) {
                std::cerr << "Error parsing sensor data: " << e.what() << "\n";
            }
        }
    }

    if (_sensorData.empty()) {
        std::cerr << "No valid sensor data loaded from " << _filename << std::endl;
    } else {
        std::cout << "Loaded " << _sensorData.size() << " sensor data points" << std::endl;
    }
}

void CsvSensorSource::run() {
    if (_sensorData.empty()) {
        std::cerr << "No sensor data available to simulate" << std::endl;
        return;
    }

    auto lastUpdate = std::chrono::steady_clock::now();
    const size_t dataSize = _sensorData.size();

    while (_running) {
        auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _startTime).count();

        // 경과 시간 기반의 현재 데이터 인덱스 계산 (초당 1개의 데이터 인덱스)
        size_t currentIndex = (elapsed / 1000) % dataSize;

        if (currentIndex != _currentIndex) {
            _currentIndex = currentIndex;
            // 현재 타임스탬프를 반영한 센서 데이터 복사
            SensorData data = _sensorData[_currentIndex];
            // 타임스탬프를 현재 시간으로 갱신
            data.timestamp = std::chrono::steady_clock::now();
            _queue.push(std::move(data));

            // debug
            /*if (std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count() >= 1) {
                std::cout << "Sensor data update - "
                          << "Index("<< _currentIndex << "/" << (dataSize-1) <<"), "
                          << "Temp("<< data.temperature <<"°C), "
                          << "Humidity("<< data.humidity <<"%), "
                          << "Accel("<< data.acceleration <<"g)" << std::endl;
                lastUpdate = now;
            }*/
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}