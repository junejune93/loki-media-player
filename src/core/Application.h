#pragma once

#include "../gl_common.h"
#include "core/MediaPlayer.h"
#include "ui/ControlPanel.h"
#include "ui/UIManager.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <string>
#include <vector>

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

private:
    GLFWwindow *_window = nullptr;
    std::unique_ptr<MediaPlayer> _mediaPlayer;
    std::unique_ptr<ControlPanel> _controlPanel;
    std::unique_ptr<UIManager> _uiManager;

    static constexpr int VIDEO_WIDTH = 1280;
    static constexpr int VIDEO_HEIGHT = 720;
    static constexpr int CONTROLS_HEIGHT = 80;

    int _windowWidth = VIDEO_WIDTH;
    int _windowHeight = VIDEO_HEIGHT + CONTROLS_HEIGHT;

    std::vector<std::string> _videoFiles;
    std::string _selectedFile;
    bool _fileLoaded = false;
};