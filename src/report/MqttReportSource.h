#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mqtt/async_client.h>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include "report/interface/IReportSource.h"

class MqttReportSource final : public IReportSource {
public:
    using MessageCallback = std::function<void(const std::string &topic, const std::string &payload)>;

    MqttReportSource(const std::string &serverAddress, const std::string &clientId);

    ~MqttReportSource() override;

    void start() override;

    void stop() override;

    void updateChannelStatus(const std::vector<ChannelStatus> &channels) override;

    void updateSyncStatus(const SyncStatus &status) override;

    void updateSensorStatus(const SensorStatus &status) override;

private:
    class MqttCallback final : public mqtt::callback {
    public:
        explicit MqttCallback(MqttReportSource &parent) : _parent(parent) {}

        void message_arrived(const mqtt::const_message_ptr msg) override {
            if (_parent._messageCallback) {
                _parent._messageCallback(msg->get_topic(), msg->get_payload_str());
            }
        }

    private:
        MqttReportSource &_parent;
    };

    void connect();

    void disconnect() const;

    bool isConnected() const;

    void publish(const std::string &topic, const std::string &payload, int qos = 1) const;

    void subscribe(const std::string &topic, int qos = 1) const;

    void setMessageCallback(MessageCallback callback);

    void sendStatus(const std::string &statusJson) const;

    // MQTT
    std::string _serverAddress;
    std::string _clientId;
    std::unique_ptr<mqtt::async_client> _client;
    MqttCallback _callback;
    MessageCallback _messageCallback;

    // Status storage
    std::vector<ChannelStatus> _channels;
    SyncStatus _syncStatus{};
    SensorStatus _sensorStatus{};
    std::atomic<bool> _running{false};
    std::mutex _mutex;
};
