#include "UIManager.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

UIManager::UIManager() = default;

UIManager::~UIManager() {
    shutdown();
}

bool UIManager::initialize(GLFWwindow *window) {
    if (_initialized) {
        return true;
    }

    _initialized = true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    setupStyle();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    _controlPanel = std::make_unique<ControlPanel>(1280, 80);

    _fileSelector = std::make_unique<FileSelector>();
    _fileSelector->setFiles({
                                    "../assets/big_buck_bunny_1080p_h264.mov",
                                    "../assets/tears_of_steel_1080p_h264.mov",
                                    "../assets/STARCRAFT_1080p_h264.mov"
                            });

    if (_onFileSelected) {
        _fileSelector->setSelectCallback(_onFileSelected);
    }

    _osdRenderer = std::make_unique<OSDRenderer>();
    return true;
}

void UIManager::setOnFileSelected(const std::function<void(const std::string &)> &cb) {
    _onFileSelected = cb;
    if (_fileSelector) {
        _fileSelector->setSelectCallback([this, cb](const std::string &file) {
            cb(file);
            _fileSelector->setVisible(false);
        });
    }
}

void UIManager::setupStyle() {
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();

    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ItemSpacing = ImVec2(8, 8);
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 4);
    style.GrabRounding = 4.0f;

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
}

void UIManager::newFrame() const {
    if (!_initialized) {
        return;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::render() {
    if (!_initialized) {
        return;
    }

    // OSD 렌더링 (다른 UI 요소들보다 먼저)
    if (_osdRenderer && _osdState.visible) {
        _osdState.updateVolumeFromSystem();
        _osdRenderer->render(_osdState, _windowWidth, _windowHeight);
    }

    if (_fileSelector && _fileSelector->isVisible()) {
        _fileSelector->render();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UIManager::updateOSDData(const MediaState& mediaState, const std::string& fileName) {
    if (!_initialized) return;

    _osdState.currentTime = mediaState.currentTime;
    _osdState.totalDuration = mediaState.duration;
    _osdState.isPlaying = mediaState.isPlaying;
    _osdState.playbackSpeed = mediaState.playbackSpeed;
    _osdState.volumeLevel = mediaState.volume;
    _osdState.isBuffering = mediaState.isBuffering;

    if (!fileName.empty()) {
        _osdState.fileName = OSDState::extractFileName(fileName);
    }

    _osdState.syncStatus = (mediaState.audioVideoSyncOffset < 40) ? "Synced" : "Out of Sync";
}

void UIManager::handleOSDInput(GLFWwindow* window) {
    if (_osdRenderer) {
        _osdRenderer->handleInput(window, _osdState);
    }
}

void UIManager::setOSDVisible(bool visible) {
    _osdState.visible = visible;
    if (visible) {
        _osdState.updateInteraction();
    }
}

void UIManager::setWindowSize(int width, int height) {
    _windowWidth = width;
    _windowHeight = height;
}

void UIManager::shutdown() {
    if (!_initialized) {
        return;
    }

    _controlPanel.reset();
    _fileSelector.reset();
    _osdRenderer.reset();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    _initialized = false;
}