#pragma once

#include <GLFW/glfw3.h>
#include <memory>
#include <string>
#include <vector>
#include "../gl_common.h"
#include "core/MediaPlayer.h"
#include "report/HttpReportSource.h"
#include "report/MqttReportSource.h"
#include "report/interface/IReportSource.h"
#include "sensors/CsvSensorSource.h"
#include "sensors/interface/ISensorSource.h"
#include "ui/ControlPanel.h"
#include "ui/UIManager.h"

class Application {
public:
    Application();

    ~Application();

    bool initialize();

    void run();

private:
    bool initializeWindow();

    static bool initializeOpenGL();

    bool initializeUI();

    void setupUICallbacks();

    void handleEvents();

    void update();

    void render();

    void cleanup();

    // OSD
    void updateOSDData();

    // Update all active reporters
    void updateReporters();

private:
    GLFWwindow *_window = nullptr;
    std::unique_ptr<MediaPlayer> _mediaPlayer;
    std::unique_ptr<ControlPanel> _controlPanel;
    std::unique_ptr<UIManager> _uiManager;
    std::unique_ptr<HttpReportSource> _httpReporter;
    std::unique_ptr<MqttReportSource> _mqttReporter;

    static constexpr int VIDEO_WIDTH = 1280;
    static constexpr int VIDEO_HEIGHT = 720;
    static constexpr int CONTROLS_HEIGHT = 80;

    int _windowWidth = VIDEO_WIDTH;
    int _windowHeight = VIDEO_HEIGHT + CONTROLS_HEIGHT;

    std::vector<std::string> _videoFiles;
    std::string _selectedFile;
    bool _fileLoaded = false;

    // Sensor
    std::unique_ptr<ISensorSource> _sensorSource;
    SensorData _latestSensorData;

    // Report
    SensorStatus _sensorStatus{};
    SyncStatus _syncStatus{};
    std::vector<ChannelStatus> _channelStatus;
};