#include "MqttReportSource.h"

MqttReportSource::MqttReportSource(const std::string &serverAddress, const std::string &clientId) :
    _serverAddress(serverAddress), _clientId(clientId),
    _client(std::make_unique<mqtt::async_client>(_serverAddress, _clientId)), _callback(*this) {
    _client->set_callback(_callback);
}

MqttReportSource::~MqttReportSource() { stop(); }

void MqttReportSource::start() {
    if (_running) {
        return;
    }

    _running = true;
    connect();
}

void MqttReportSource::stop() {
    if (!_running) {
        return;
    }

    _running = false;
    disconnect();
}

void MqttReportSource::updateChannelStatus(const std::vector<ChannelStatus> &channels) {
    std::lock_guard<std::mutex> lock(_mutex);
    _channels = channels;

    // state send
    auto updateAndSend = [this]() {
        const std::string statusJson = buildStatusJson(_channels, _syncStatus, _sensorStatus);
        sendStatus(statusJson);
    };
    updateAndSend();
}

void MqttReportSource::updateSyncStatus(const SyncStatus &status) {
    std::lock_guard<std::mutex> lock(_mutex);
    _syncStatus = status;

    // state send
    auto updateAndSend = [this]() {
        const std::string statusJson = buildStatusJson(_channels, _syncStatus, _sensorStatus);
        sendStatus(statusJson);
    };
    updateAndSend();
}

void MqttReportSource::updateSensorStatus(const SensorStatus &status) {
    std::lock_guard<std::mutex> lock(_mutex);
    _sensorStatus = status;

    // state send
    auto updateAndSend = [this]() {
        const std::string statusJson = buildStatusJson(_channels, _syncStatus, _sensorStatus);
        sendStatus(statusJson);
    };
    updateAndSend();
}

void MqttReportSource::connect() {
    if (isConnected()) {
        return;
    }

    auto attemptConnection = [this]() {
        try {
            mqtt::connect_options connOpts;
            connOpts.set_keep_alive_interval(20);
            connOpts.set_clean_session(true);

            const auto token = _client->connect(connOpts);
            token->wait();
            spdlog::info("Connected to MQTT broker at {}", _serverAddress);
            return true;
        } catch (const mqtt::exception &e) {
            spdlog::error("MQTT connection failed: {}", e.what());
            return false;
        }
    };

    attemptConnection();
}

void MqttReportSource::disconnect() const {
    if (!_client || !_client->is_connected()) {
        return;
    }

    auto attemptDisconnection = [this]() {
        try {
            _client->disconnect()->wait();
            spdlog::info("Disconnected from MQTT broker");
            return true;
        } catch (const mqtt::exception &e) {
            spdlog::error("MQTT disconnect failed: {}", e.what());
            return false;
        }
    };

    attemptDisconnection();
}

bool MqttReportSource::isConnected() const { return _client && _client->is_connected(); }

void MqttReportSource::publish(const std::string &topic, const std::string &payload, int qos) const {
    if (!_running || !isConnected()) {
        spdlog::warn("Cannot publish: MQTT client is not connected");
        return;
    }

    // publish
    auto publish = [this](const std::string &topic, const std::string &payload, const int qos) {
        try {
            const auto message = mqtt::make_message(topic, payload, qos, false);
            _client->publish(message)->wait();
            return true;
        } catch (const mqtt::exception &e) {
            spdlog::error("MQTT publish failed: {}", e.what());
            return false;
        }
    };

    publish(topic, payload, qos);
}

void MqttReportSource::subscribe(const std::string &topic, const int qos) const {
    if (!_running || !isConnected()) {
        spdlog::warn("Cannot subscribe: MQTT client is not connected");
        return;
    }

    // subscribe
    auto subscribe = [this](const std::string &topic, const int qos) {
        try {
            _client->subscribe(topic, qos)->wait();
            spdlog::info("Subscribed to topic: {}", topic);
            return true;
        } catch (const mqtt::exception &e) {
            spdlog::error("MQTT subscribe failed: {}", e.what());
            return false;
        }
    };

    subscribe(topic, qos);
}

void MqttReportSource::setMessageCallback(MessageCallback callback) { _messageCallback = std::move(callback); }

void MqttReportSource::sendStatus(const std::string &statusJson) const { publish("status", statusJson); }