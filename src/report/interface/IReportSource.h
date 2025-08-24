#pragma once

#include <memory>
#include <string>
#include <vector>

struct ChannelStatus {
    int id;
    int fps;
    int queue_length;
};

struct SyncStatus {
    double max_offset_ms;
    bool locked;
};

struct SensorStatus {
    double temperature;
    double humidity;
    double acceleration;
};

class IReportSource {
public:
    virtual ~IReportSource() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual void updateChannelStatus(const std::vector<ChannelStatus> &channels) = 0;

    virtual void updateSyncStatus(const SyncStatus &status) = 0;

    virtual void updateSensorStatus(const SensorStatus &status) = 0;

protected:
    virtual std::string buildStatusJson(const std::vector<ChannelStatus> &channels, const SyncStatus &syncStatus,
                                        const SensorStatus &sensorStatus) const;
};