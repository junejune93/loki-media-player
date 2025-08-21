#include "OSDRenderer.h"
#include "imgui.h"
#include <cmath>

OSDRenderer::OSDRenderer() {
    _lastStateChange = std::chrono::steady_clock::now();
}

OSDRenderer::~OSDRenderer() = default;

void OSDRenderer::render(const OSDState &state, int windowWidth, int windowHeight) {
    if (!state.visible) {
        return;
    }

    setupOSDStyle(state.fadeAlpha);

    // PLAY INFO
    renderPlaybackInfo(state, windowWidth);

    // CODEC INFO
    renderCodecInfo(state, windowWidth, windowHeight);

    // SENSOR INFO
    renderSensorInfo(state, windowWidth, windowHeight);

    // PLAYER STATUS
    renderSyncInfo(state, windowWidth, windowHeight);
    
    renderCenterStatus(state, windowWidth, windowHeight);

    restoreOSDStyle();
}

void OSDRenderer::handleInput(GLFWwindow *window, OSDState &state) {
    // key input
    bool oKeyDown = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
    bool iKeyDown = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
    bool sKeyDown = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool cKeyDown = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;

    // key: O
    if (oKeyDown && !_oKeyPressed) {
        state.visible = !state.visible;
        state.updateInteraction();
    }
    _oKeyPressed = oKeyDown;

    // key: I
    if (iKeyDown && !_iKeyPressed) {
        state.showPlaybackInfo = !state.showPlaybackInfo;
        state.updateInteraction();
    }
    _iKeyPressed = iKeyDown;

    // key: S
    if (sKeyDown && !_sKeyPressed) {
        state.showStatusInfo = !state.showStatusInfo;
        state.updateInteraction();
    }
    _sKeyPressed = sKeyDown;

    // key: C
    if (cKeyDown && !_cKeyPressed) {
        state.showCodecInfo = !state.showCodecInfo;
        state.updateInteraction();
    }
    _cKeyPressed = cKeyDown;

    // mouse
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if (std::abs(mouseX - _lastMouseX) > 5.0 || std::abs(mouseY - _lastMouseY) > 5.0) {
        state.updateInteraction();
        _lastMouseX = mouseX;
        _lastMouseY = mouseY;
    }
}

void OSDRenderer::renderPlaybackInfo(const OSDState &state, int windowWidth) {
    const float windowHeight = 100.0f;
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f));

    // 파일명 길이에 따라 최소 너비 계산
    float minWidth = 200.0f;
    if (!state.fileName.empty()) {
        ImVec2 textSize = ImGui::CalcTextSize(state.fileName.c_str());
        minWidth = std::max(minWidth, textSize.x + 70.0f);
    }

    ImGui::SetNextWindowSize(ImVec2(minWidth, windowHeight));
    ImGui::Begin("##PlaybackInfo", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoNav);

    // HEADER
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "PLAY INFO");
    ImGui::PopFont();

    ImGui::Separator();

    if (!state.fileName.empty()) {
        ImGui::Text("[FILE] %s", state.fileName.c_str());
    }

    std::string currentTimeStr = OSDState::formatTime(state.currentTime);
    std::string totalTimeStr = OSDState::formatTime(state.totalDuration);
    ImGui::Text("[TIME] %s / %s", currentTimeStr.c_str(), totalTimeStr.c_str());

    ImGui::Text("[SPEED] %.1fx", state.playbackSpeed);
    ImGui::Text("[VOLUME] %.0f%%", state.volumeLevel * 100.0f);

    ImGui::End();
}

void OSDRenderer::renderSyncInfo(const OSDState &state, int windowWidth, int windowHeight) {
    const float windowPadding = 20.0f;
    const float sensorWindowHeight = 140.0f;
    const float windowWidthPx = 250.0f;
    const float windowHeightPx = 70.0f;
    const float startY = windowPadding + sensorWindowHeight + 20.0f;

    ImGui::SetNextWindowPos(ImVec2(
            (float)windowWidth - windowWidthPx - windowPadding,
            startY));

    ImGui::SetNextWindowSize(ImVec2(windowWidthPx, windowHeightPx));
    ImGui::Begin("##StatusInfo", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoNav);

    // HEADER
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "PLAYER STATUS");
    ImGui::PopFont();

    ImGui::Separator();

    const char *playIcon = state.isPlaying ? "[PAUSE]" : "[PLAY]";
    const char *playText = state.isPlaying ? "Playing" : "Paused";
    ImGui::Text("%s %s", playIcon, playText);

    const char *syncIcon1 = (state.syncStatus == "Synced") ? "[SYNC]" : "[WARN]";
    ImGui::Text("%s %s", syncIcon1, state.syncStatus.c_str());

    ImGui::End();
}

void OSDRenderer::renderCodecInfo(const OSDState &state, int windowWidth, int windowHeight) {
    const float codecWindowWidth = 200.0f;
    const float codecWindowHeight = 170.0f;
    const float startY = 140.0f;
    
    ImGui::SetNextWindowPos(ImVec2(20.0f, startY));
    ImGui::SetNextWindowSize(ImVec2(codecWindowWidth, codecWindowHeight));
    
    ImGui::Begin("##CodecInfo", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoNav);

    // HEADER
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "CODEC INFO");
    ImGui::PopFont();

    ImGui::Separator();

    const auto &codec = state.codecInfo;

    if (!codec.containerFormat.empty()) {
        ImGui::Text("[FORMAT] %s", codec.containerFormat.c_str());
    }

    // video
    if (codec.hasVideo) {
        if (!codec.videoCodec.empty()) {
            ImGui::Text("[VIDEO] %s", codec.videoCodec.c_str());
        }
        if (!codec.videoResolution.empty()) {
            ImGui::Text("[RES] %s", codec.videoResolution.c_str());
        }
        if (!codec.videoBitrate.empty()) {
            ImGui::Text("[V-BITRATE] %s", codec.videoBitrate.c_str());
        }
    }

    // audio
    if (codec.hasAudio) {
        if (!codec.audioCodec.empty()) {
            ImGui::Text("[AUDIO] %s", codec.audioCodec.c_str());
        }
        if (!codec.audioSampleRate.empty()) {
            ImGui::Text("[SAMPLE] %s", codec.audioSampleRate.c_str());
        }
        if (!codec.audioBitrate.empty()) {
            ImGui::Text("[A-BITRATE] %s", codec.audioBitrate.c_str());
        }
        if (!codec.audioChannels.empty()) {
            ImGui::Text("[CHANNELS] %s", codec.audioChannels.c_str());
        }
    }

    ImGui::End();
}

void OSDRenderer::renderCenterStatus(const OSDState &state, int windowWidth, int windowHeight) {
    if (_lastPlayingState != state.isPlaying) {
        _lastStateChange = std::chrono::steady_clock::now();
        _lastPlayingState = state.isPlaying;
    }

    auto timeSinceChange = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - _lastStateChange).count();

    if (timeSinceChange < 1500) {
        float centerAlpha = std::max(0.0f, 1.0f - ((float) timeSinceChange / 1500.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f * centerAlpha));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, centerAlpha));

        ImGui::SetNextWindowPos(ImVec2((float) windowWidth / 2 - 50, (float) windowHeight / 2 - 30));
        ImGui::Begin("##CenterStatus", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav);

        ImGui::SetWindowFontScale(2.0f);
        ImGui::Text(state.isPlaying ? "PAUSE" : "PLAY");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::End();
        ImGui::PopStyleColor(2);
    }
}

void OSDRenderer::setupOSDStyle(float alpha) {
    // OSD
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

    // Bg, Text
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f * alpha));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.5f * alpha));
}

void OSDRenderer::renderSensorInfo(const OSDState &state, int windowWidth, int windowHeight) {
    const float sensorWindowWidth = 250.0f;
    const float sensorWindowHeight = 145.0f;
    const float startY = 20.0f;

    ImGui::SetNextWindowPos(ImVec2(
            (float)windowWidth - sensorWindowWidth - 20.0f,
            startY
    ));
    ImGui::SetNextWindowSize(ImVec2(sensorWindowWidth, sensorWindowHeight));

    ImGui::Begin("##SensorInfo", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoNav);

    // HEADER
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "SENSOR INFO");
    ImGui::PopFont();

    ImGui::Separator();

    if (!state.sensorReadings.source.empty()) {
        ImGui::Text("[SOURCE] %s", state.sensorReadings.source.c_str());
    }

    std::string updated = state.sensorReadings.getFormattedTimeSinceUpdate();
    if (!updated.empty()) {
        ImGui::Text("[UPDATED] %s", updated.c_str());
    }

    ImGui::Text("[TEMP] %.1f °C", state.sensorReadings.temperature);

    ImGui::Text("[HUM] %.1f %%", state.sensorReadings.humidity);

    ImGui::Text("[ACCEL] %.2f g", state.sensorReadings.acceleration);

    float accelBar = std::min(1.0f, (float)state.sensorReadings.acceleration / 2.0f);
    ImGui::ProgressBar(accelBar, ImVec2(-1, 20), "");

    ImGui::End();
}

void OSDRenderer::restoreOSDStyle() {
    ImGui::PopStyleColor(3 /* WindowBg, Text, Border */);
    ImGui::PopStyleVar(3 /* WindowRounding, WindowPadding, ItemSpacing */);
}