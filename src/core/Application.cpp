#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Application.h"
#include "core/Utils.h"
#include "imgui.h"
#include <iostream>

Application::Application() {
    _videoFiles = {
            "../assets/big_buck_bunny_1080p_h264.mov",
            "../assets/tears_of_steel_1080p_h264.mov",
            "../assets/STARCRAFT_1080p_h264.mov"
    };
    _selectedFile.clear();
    _fileLoaded = false;
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

    _mediaPlayer = std::make_unique<MediaPlayer>();
    if (!_mediaPlayer->initialize(_window, VIDEO_WIDTH, VIDEO_HEIGHT)) {
        std::cerr << "Failed to initialize media player\n";
        return false;
    }

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

    return true;
}

void Application::setupUICallbacks() {
    _controlPanel->setPlayCallback([this]() { _mediaPlayer->play(); });
    _controlPanel->setPauseCallback([this]() { _mediaPlayer->pause(); });
    _controlPanel->setStopCallback([this]() { _mediaPlayer->stop(); });
    _controlPanel->setSeekCallback([this](double time) { _mediaPlayer->seek(time); });
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
}

void Application::update() {
    _mediaPlayer->update();

    if (!_fileLoaded && !_selectedFile.empty()) {
        if (_mediaPlayer->loadFile(_selectedFile)) {
            std::cout << "Playing: " << _selectedFile << "\n";
            _fileLoaded = true;
        }
    }
}

void Application::render() {
    // Start new ImGui frame
    _uiManager->newFrame();

    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render video
    _mediaPlayer->render(_windowWidth, _windowHeight, CONTROLS_HEIGHT);

    // Render UI controls
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

    // Render FileSelector and other UI
    if (_uiManager) {
        _uiManager->render();
    }

    // Swap buffers
    glfwSwapBuffers(_window);
}

void Application::cleanup() {
    if (_mediaPlayer) {
        _mediaPlayer->stop();
        _mediaPlayer.reset();
    }

    _controlPanel.reset();
    _uiManager.reset();

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    if (_window) {
        glfwDestroyWindow(_window);
        _window = nullptr;
    }

    glfwTerminate();
}