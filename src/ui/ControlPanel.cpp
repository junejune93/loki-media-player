#include <algorithm>
#include "ControlPanel.h"
#include "../core/Utils.h"

ControlPanel::ControlPanel(int videoWidth, int controlsHeight)
        : _videoWidth(videoWidth),
          _controlsHeight(controlsHeight) {
}

void ControlPanel::render(MediaState &state) {
    ImGui::SetNextWindowPos(ImVec2(0, static_cast<float>(_videoWidth * 720 / 1280)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(_videoWidth), static_cast<float>(_controlsHeight)),
                             ImGuiCond_Always);

    ImGui::Begin("MediaControls", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);

    renderProgressBar(state);
    renderTimeDisplay(state);
    renderControlButtons(state);

    ImGui::End();
}

void ControlPanel::renderProgressBar(MediaState &state) {
    ImGui::SetCursorPosY(8);
    auto progress = static_cast<float>(state.getProgress());
    float progressValue = progress;

    ImGui::PushItemWidth(static_cast<float>(_videoWidth) - 240);
    ImGui::SetCursorPosX(12);

    if (ImGui::SliderFloat("##progress", &progressValue, 0.0f, 1.0f, "")) {
        if (ImGui::IsItemActive()) {
            double seekTime = progressValue * state.totalDuration;
            if (_onSeek) {
                _onSeek(seekTime);
            }
        }
    }
    ImGui::PopItemWidth();
}

void ControlPanel::renderTimeDisplay(const MediaState &state) {
    ImGui::SameLine();
    std::string timeText = Utils::formatTime(state.currentTime) + " / " + Utils::formatTime(state.totalDuration);
    ImGui::TextUnformatted(timeText.c_str());
}

void ControlPanel::renderControlButtons(const MediaState &state) {
    ImGui::SetCursorPosY(35);

    const float buttonWidth = 60.0f;
    const float buttonHeight = 35.0f;
    const float spacing = 15.0f;
    const int numButtons = 3;
    float totalWidth = buttonWidth * numButtons + spacing * (numButtons - 1);
    float startX = (static_cast<float>(_videoWidth) - totalWidth) * 0.5f;

    ImGui::SetCursorPosX(startX);

    // Play/Pause Toggle
    const char *playPauseLabel = state.isPlaying ? "Pause" : "Play";
    if (ImGui::Button(playPauseLabel, ImVec2(buttonWidth, buttonHeight))) {
        if (state.isPlaying) {
            if (_onPause) {
                _onPause();
            }
        } else {
            if (_onPlay) {
                _onPlay();
            }
        }
    }

    ImGui::SameLine(0, spacing);
    if (ImGui::Button("Stop", ImVec2(buttonWidth, buttonHeight))) {
        if (_onStop) {
            _onStop();
        }
    }

    ImGui::SameLine(0, spacing);
    if (ImGui::Button("<<10s", ImVec2(buttonWidth, buttonHeight))) {
        double seekTime = std::max(0.0, state.currentTime - 10.0);
        if (_onSeek) {
            _onSeek(seekTime);
        }
    }
}