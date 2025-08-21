#pragma once

#include "sensors/SensorData.h"
#include "media/ThreadSafeQueue.h"
#include <atomic>
#include <thread>

class ISensorSource {
public:
    virtual ~ISensorSource() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual void flush() = 0;

    virtual ThreadSafeQueue<SensorData> &getQueue() = 0;
};