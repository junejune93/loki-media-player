#include "report/HttpReportSource.h"
#include <chrono>
#include <cpprest/json.h>

using namespace std::chrono_literals;

HttpReportSource::HttpReportSource(const std::string &serverUrl, const std::string &endpoint) : _endpoint(endpoint) {
    cpprest_client::Config config;
    config.base_url = serverUrl;
    config.user_agent = "loki-media-player/1.0";
    config.add_default_header("Content-Type", "application/json");
    config.enable_connection_pool = true;
    config.enable_keep_alive = true;

    _httpClient = std::make_unique<cpprest_client::HttpClientImpl>(config);
}

HttpReportSource::~HttpReportSource() { stop(); }

void HttpReportSource::start() {
    // Already running
    if (_running.exchange(true)) {
        return;
    }

    auto reportingLambda = [this]() {
        auto nextReportTime = std::chrono::steady_clock::now();
        while (_running) {
            nextReportTime += 20s;

            try {
                std::vector<ChannelStatus> channels;
                SyncStatus syncStatus;
                SensorStatus sensorStatus;

                // collection - status
                auto collectStatus = [this, &channels, &syncStatus, &sensorStatus]() {
                    std::lock_guard<std::mutex> lock(_statusMutex);
                    channels = _channels;
                    syncStatus = _syncStatus;
                    sensorStatus = _sensorStatus;
                };

                collectStatus();

                if (_running) {
                    // set data - json
                    std::string statusJson = buildStatusJson(channels, syncStatus, sensorStatus);

                    // send data - http
                    sendStatus(statusJson);
                }
            } catch (const std::exception &e) {
            }

            auto waitForNextReport = [this, nextReportTime]() {
                while (_running && std::chrono::steady_clock::now() < nextReportTime) {
                    std::this_thread::sleep_for(100ms);
                }
            };

            waitForNextReport();
        }
    };

    _reportingThread = std::thread(reportingLambda);
    spdlog::info("HTTP report source started");
}

void HttpReportSource::stop() {
    // Already stopped
    if (!_running.exchange(false)) {
        return;
    }

    if (_reportingThread.joinable()) {
        _reportingThread.join();
    }
}

void HttpReportSource::updateChannelStatus(const std::vector<ChannelStatus> &channels) {
    std::lock_guard<std::mutex> lock(_statusMutex);
    _channels = channels;
}

void HttpReportSource::updateSyncStatus(const SyncStatus &status) {
    std::lock_guard<std::mutex> lock(_statusMutex);
    _syncStatus = status;
}

void HttpReportSource::updateSensorStatus(const SensorStatus &status) {
    std::lock_guard<std::mutex> lock(_statusMutex);
    _sensorStatus = status;
}

void HttpReportSource::sendStatus(const std::string &statusJson) {
    static int consecutiveFailures = 0;

    // Request
    auto request = [this, &statusJson]() {
        spdlog::debug("Sending status update: {}", statusJson);
        const auto startTime = std::chrono::steady_clock::now();
        const auto response = _httpClient->post(_endpoint, statusJson);
        const auto endTime = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        spdlog::info("API Response - Duration: {}, Status: {}", duration, response.status_code);
        spdlog::info("Response Body: {}", response.body);
        return response;
    };

    // Response
    auto response = [](const auto &response) {
        static int &failures = consecutiveFailures;
        if (response.status_code >= 200 && response.status_code < 300) {
            if (failures > 0) {
                spdlog::info("Successfully connected to API after {} failures", failures);
                failures = 0;
            }
        } else {
            failures++;
        }
    };

    // Error
    auto handleError = [](const std::exception &e) { consecutiveFailures++; };

    try {
        auto requestResult = request();
        response(requestResult);
    } catch (const std::exception &e) {
        handleError(e);
    }
}

std::string IReportSource::buildStatusJson(const std::vector<ChannelStatus> &channels, const SyncStatus &syncStatus,
                                           const SensorStatus &sensorStatus) const {
    web::json::value result;

    // status - channel
    auto buildChannelJson = [&channels]() {
        web::json::value channelArray = web::json::value::array();
        for (size_t i = 0; i < channels.size(); ++i) {
            web::json::value channelObj;
            channelObj[U("id")] = channels[i].id;
            channelObj[U("fps")] = channels[i].fps;
            channelObj[U("queue_length")] = channels[i].queue_length;
            channelArray[i] = channelObj;
        }
        return channelArray;
    };

    // status - sync
    auto buildSyncJson = [&syncStatus]() {
        web::json::value syncObj;
        syncObj[U("max_offset_ms")] = syncStatus.max_offset_ms;
        syncObj[U("locked")] = syncStatus.locked;
        return syncObj;
    };

    // status - sensor
    auto buildSensorJson = [&sensorStatus]() {
        web::json::value sensorObj;
        sensorObj[U("temperature")] = sensorStatus.temperature;
        sensorObj[U("humidity")] = sensorStatus.humidity;
        sensorObj[U("acceleration")] = sensorStatus.acceleration;
        return sensorObj;
    };

    // combine
    auto combineJson = [&result](const auto &channelArray, const auto &syncObj, const auto &sensorObj) {
        result[U("channel_status")] = channelArray;
        result[U("sync_status")] = syncObj;
        result[U("sensor_status")] = sensorObj;
        return result.serialize();
    };

    return combineJson(buildChannelJson(), buildSyncJson(), buildSensorJson());
}
