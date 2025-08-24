#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Application.h"
#include "imgui.h"
#include "report/HttpReportSource.h"
#include <iostream>
#include <filesystem>

Application::Application() {
    _videoFiles.clear();
    std::string assetsPath = "../assets/";
    for (const auto &entry: std::filesystem::directory_iterator(assetsPath)) {
        if (entry.is_regular_file()) {
            _videoFiles.push_back(entry.path().string());
        }
    }
    _selectedFile.clear();
    _fileLoaded = false;

    // Load Sensor Info (CSV)
    std::string csvPath = "../assets/sensor_data.csv";
    _sensorSource = std::make_unique<CsvSensorSource>(csvPath);
}

Application::~Application() {
    cleanup();
}

bool Application::initialize() {
    if (!initializeWindow()) {
        return false;
    }

    if (!initializeOpenGL()) {
        return false;
    }

    if (!initializeUI()) {
        return false;
    }

    // Report - status
    try {
        _statusReporter = std::make_unique<HttpReportSource>("https://httpbin.org", "/post");
        // values (TEST)
        _channelStatus = {
                // id, fps, queue_length
            {0, 30, 5},
            {1, 30, 3}
        };
        _syncStatus = {0.0, false};
        _sensorStatus = {0.0, 0.0, 0.0};
        
        _statusReporter->updateChannelStatus(_channelStatus);
        _statusReporter->updateSyncStatus(_syncStatus);
        _statusReporter->updateSensorStatus(_sensorStatus);
        _statusReporter->start();
        spdlog::info("Status reporter initialized successfully");
    } catch (const std::exception &e) {
        spdlog::error("Failed to initialize status reporter: {}", e.what());
        return false;
    }

    // Sensor Info - collection
    try {
        _sensorSource->start();
        std::cout << "Sensor source initialized successfully\n";
    } catch (const std::exception &e) {
        std::cerr << "Failed to initialize sensor source: " << e.what() << "\n";
        return false;
    }

    // Media Player
    _mediaPlayer = std::make_unique<MediaPlayer>();
    if (!_mediaPlayer->initialize(_window, VIDEO_WIDTH, VIDEO_HEIGHT)) {
        std::cerr << "Failed to initialize media player\n";
        return false;
    }

    // Media Control Panel
    _controlPanel = std::make_unique<ControlPanel>(VIDEO_WIDTH, CONTROLS_HEIGHT);
    setupUICallbacks();

    std::cout << "Application initialized. Use 'Open Video' button to load a file.\n";
    return true;
}

bool Application::initializeWindow() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(_windowWidth, _windowHeight,
                               "Loki Media Player", nullptr, nullptr);
    if (!_window) {
        std::cerr << "Failed to create _window\n";
        glfwTerminate();
        return false;
    }

    GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
    if (primaryMonitor) {
        const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);
        if (mode) {
            int xpos = (mode->width - _windowWidth) / 2;
            int ypos = (mode->height - _windowHeight) / 2;
            glfwSetWindowPos(_window, xpos, ypos);
        }
    }

    glfwMakeContextCurrent(_window);
    glfwSwapInterval(1);
    return true;
}

bool Application::initializeOpenGL() {
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return false;
    }
    return true;
}

bool Application::initializeUI() {
    _uiManager = std::make_unique<UIManager>();
    if (!_uiManager->initialize(_window)) {
        std::cerr << "Failed to initialize UIManager\n";
        return false;
    }

    _uiManager->setOnFileSelected([this](const std::string &file) {
        _selectedFile = file;
        _fileLoaded = false;
    });

    // OSD
    _uiManager->setWindowSize(_windowWidth, _windowHeight);

    return true;
}

void Application::setupUICallbacks() {
    _controlPanel->setPlayCallback([this]() { _mediaPlayer->play(); });
    _controlPanel->setPauseCallback([this]() { _mediaPlayer->pause(); });
    _controlPanel->setStopCallback([this]() { _mediaPlayer->stop(); });
    _controlPanel->setSeekCallback([this](double time) { _mediaPlayer->seek(time); });
    _controlPanel->setStartRecordingCallback([this]() -> bool {
        std::string recordingsDir = "record";
        if (!std::filesystem::exists(recordingsDir)) {
            std::filesystem::create_directory(recordingsDir);
        }
        
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
        std::string filename = recordingsDir + "/record_" + ss.str() + ".mp4";
        
        const auto started = _mediaPlayer->startRecording(recordingsDir);
        if (started) {
            std::cout << "Started recording to: " << filename << std::endl;
        }
        return started;
    });
    
    _controlPanel->setStopRecordingCallback([this]() {
        _mediaPlayer->stopRecording();
    });
    
    _mediaPlayer->setOnRecordingStateChanged([this](bool isRecording) {
        _controlPanel->setRecordingState(isRecording);
    });
}

void Application::run() {
    while (!glfwWindowShouldClose(_window)) {
        handleEvents();
        update();
        render();
    }
}

void Application::handleEvents() {
    glfwPollEvents();

    // OSD
    if (_uiManager) {
        _uiManager->handleOSDInput(_window);
    }
}

void Application::update() {
    _mediaPlayer->update();

    if (!_fileLoaded && !_selectedFile.empty()) {
        if (_mediaPlayer->loadFile(_selectedFile)) {
            std::cout << "Playing: " << _selectedFile << "\n";
            _fileLoaded = true;
        }
    }
    
    // Report
    if (_statusReporter) {
        if (_mediaPlayer) {
            auto state = _mediaPlayer->getState();
            _channelStatus[0].fps = 30;
            _channelStatus[0].queue_length = 0;
            _statusReporter->updateChannelStatus(_channelStatus);
            
            _syncStatus.max_offset_ms = state.audioVideoSyncOffset * 1000.0; // ms
            _syncStatus.locked = state.isPlaying && (std::abs(state.audioVideoSyncOffset) < 0.1);
            _statusReporter->updateSyncStatus(_syncStatus);
        }
    }

    // OSD (Sensor)
    if (_sensorSource) {
        static auto lastSensorUpdate = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        // Update sensor data every 100ms
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastSensorUpdate).count() >= 100) {

            SensorData sensorData;
            if (_sensorSource->getQueue().tryPop(sensorData)) {
                _latestSensorData = sensorData;

                // Update Report (Sensor)
                if (_statusReporter) {
                    _sensorStatus.temperature = _latestSensorData.temperature;
                    _sensorStatus.humidity = _latestSensorData.humidity;
                    _sensorStatus.acceleration = _latestSensorData.acceleration;
                    _statusReporter->updateSensorStatus(_sensorStatus);
                }

                // Update UI (File Select, Codec Info, OSD)
                _uiManager->updateOSDData(_mediaPlayer->getState(),
                                        _mediaPlayer->getCodecInfo(),
                                        _selectedFile,
                                        _latestSensorData.temperature,
                                        _latestSensorData.humidity,
                                        _latestSensorData.acceleration,
                                        _latestSensorData.source);
            }

            lastSensorUpdate = now;
        }
    }

    // OSD
    if (_uiManager && _mediaPlayer) {
        updateOSDData();
    }
}

void Application::updateOSDData() {
    if (!_mediaPlayer) {
        return;
    }

    // Media Player - Info
    const auto mediaState = _mediaPlayer->getState();

    MediaState osdMediaState;
    osdMediaState.currentTime = mediaState.currentTime;
    osdMediaState.duration = mediaState.duration;
    osdMediaState.isPlaying = mediaState.isPlaying;
    osdMediaState.playbackSpeed = mediaState.playbackSpeed;
    osdMediaState.volume = mediaState.volume;
    osdMediaState.audioVideoSyncOffset = mediaState.audioVideoSyncOffset;

    const auto duration = _mediaPlayer->getDuration();
    osdMediaState.totalDuration = duration;

    // Codec - Info
    const auto codecInfo = _mediaPlayer->getCodecInfo();

    // Sensor - Info
    if (_sensorSource && _latestSensorData.source != "") {
        _uiManager->updateOSDData(osdMediaState, codecInfo, _selectedFile,
                                  _latestSensorData.temperature,
                                  _latestSensorData.humidity,
                                  _latestSensorData.acceleration,
                                  _latestSensorData.source);
    } else {
        _uiManager->updateOSDData(osdMediaState, codecInfo, _selectedFile, 0.0, 0.0, 0.0, "No sensor data");
    }
}

void Application::render() {
    _uiManager->newFrame();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render - video
    _mediaPlayer->render(_windowWidth, _windowHeight, CONTROLS_HEIGHT);

    // Render - UI
    _controlPanel->render(_mediaPlayer->getState());

    // File menu
    ImGui::Begin("File");
    if (ImGui::Button("Open Video")) {
        if (_uiManager) {
            _uiManager->getFileSelector()->setVisible(true);
        }
    }
    ImGui::Text("Selected: %s", _selectedFile.empty() ? "None" : _selectedFile.c_str());
    ImGui::End();

    // OSD
    if (_uiManager) {
        _uiManager->render();
    }

    // Swap buffers
    glfwSwapBuffers(_window);
}

void Application::cleanup() {
    if (_statusReporter) {
        _statusReporter->stop();
    }
    
    if (_sensorSource) {
        _sensorSource->stop();
    }

    if (_mediaPlayer) {
        _mediaPlayer->stop();
        _mediaPlayer.reset();
    }

    if (_uiManager) {
        _uiManager->shutdown();
    }

    if (_window) {
        glfwDestroyWindow(_window);
        _window = nullptr;
    }

    glfwTerminate();
}