#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <cpprest-client/HttpClientImpl.h>
#include <spdlog/spdlog.h>
#include "report/interface/IReportSource.h"

class HttpReportSource final : public IReportSource {
public:
    HttpReportSource(const std::string &serverUrl, const std::string &endpoint);

    ~HttpReportSource() override;

    void start() override;

    void stop() override;

    void updateChannelStatus(const std::vector<ChannelStatus> &channels) override;

    void updateSyncStatus(const SyncStatus &status) override;

    void updateSensorStatus(const SensorStatus &status) override;

private:
    void sendStatus(const std::string &statusJson) const;

    std::unique_ptr<cpprest_client::HttpClientImpl> _httpClient;
    std::string _endpoint;

    std::vector<ChannelStatus> _channels;
    SyncStatus _syncStatus{};
    SensorStatus _sensorStatus{};

    std::atomic<bool> _running{false};
    std::thread _reportingThread;
    std::mutex _statusMutex;
};