#pragma once

#include "sensors/interface/ISensorSource.h"
#include <fstream>

class CsvSensorSource : public ISensorSource {
public:
    explicit CsvSensorSource(std::string filename);

    ~CsvSensorSource() override;

    void start() override;

    void stop() override;

    void flush() override;

    ThreadSafeQueue<SensorData> &getQueue() override { return _queue; }

private:
    void run();

    void loadSensorData();

    std::string _filename;
    std::ifstream _file;
    std::atomic<bool> _running{false};
    std::thread _thread;
    ThreadSafeQueue<SensorData> _queue;
    std::vector<SensorData> _sensorData;
    std::atomic<size_t> _currentIndex{0};
    std::chrono::steady_clock::time_point _startTime;
};