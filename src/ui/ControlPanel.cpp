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

    const float progressBarWidth = static_cast<float>(_videoWidth) - 240;
    const ImVec2 progressBarPos = ImGui::GetCursorScreenPos();
    const float progressBarHeight = ImGui::GetFrameHeight();

    ImGui::PushItemWidth(progressBarWidth);
    ImGui::SetCursorPosX(12);

    if (ImGui::SliderFloat("##progress", &progressValue, 0.0f, 1.0f, "")) {
        if (ImGui::IsItemActive()) {
            double seekTime = progressValue * state.totalDuration;
            if (_onSeek) {
                _onSeek(seekTime);
            }
        }
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::SetCursorPosX(12);
    ImGui::Checkbox("Show Frame Markers", &_showMarkers);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // markers - On/Off
    if (_showMarkers) {
        const ImU32 iFrameColor = IM_COL32(255, 165, 0, 255 /* Orange */);
        const ImU32 pFrameColor = IM_COL32(50, 205, 50, 180 /* Lime Green */);

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool showTooltip = false;
        double tooltipTimestamp = 0.0;

        // markers - I-Frame
        for (const auto& timestamp : state.getIFrameTimestamps()) {
            if (timestamp <= state.totalDuration && state.totalDuration > 0) {
                float pos = static_cast<float>(timestamp / state.totalDuration) * progressBarWidth;
               ImVec2 markerPos = ImVec2(progressBarPos.x + pos, progressBarPos.y);

                // check - mouse hover
                ImVec2 markerRect1(markerPos.x - 3.0f, markerPos.y);
                ImVec2 markerRect2(markerPos.x + 3.0f, markerPos.y + progressBarHeight);
                bool isHovering = (mousePos.x >= markerRect1.x && mousePos.x <= markerRect2.x &&
                                  mousePos.y >= markerRect1.y && mousePos.y <= markerRect2.y);

                // effect - mouse hover
                if (isHovering) {
                    showTooltip = true;
                    tooltipTimestamp = timestamp;

                    drawList->AddLine(
                        ImVec2(markerPos.x, markerPos.y - 2),
                        ImVec2(markerPos.x, markerPos.y + progressBarHeight + 2),
                        IM_COL32(255, 255, 255, 100),
                        6.0f
                    );
                }

                drawList->AddLine(
                    markerPos,
                    ImVec2(markerPos.x, markerPos.y + progressBarHeight),
                    iFrameColor,
                    isHovering ? 4.0f : 3.0f
                );
            }
        }

        // markers - P-Frame
        for (const auto& timestamp : state.getPFrameTimestamps()) {
            if (timestamp <= state.totalDuration && state.totalDuration > 0) {
                float pos = static_cast<float>(timestamp / state.totalDuration) * progressBarWidth;
                drawList->AddLine(
                    ImVec2(progressBarPos.x + pos, progressBarPos.y + progressBarHeight * 0.3f),
                    ImVec2(progressBarPos.x + pos, progressBarPos.y + progressBarHeight * 0.7f),
                    pFrameColor,
                    1.5f
                );
            }
        }

        // tooltip - I-Frame timestamp
        if (showTooltip) {
            ImGui::SetNextWindowPos(ImVec2(mousePos.x + 10, mousePos.y - 30));
            ImGui::SetNextWindowSize(ImVec2(0, 0));
            ImGui::Begin("##IFrameTooltip", nullptr,
                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);

            // timestamp format(HH:MM:SS.mmm)
            int hours = static_cast<int>(tooltipTimestamp / 3600);
            int minutes = static_cast<int>((tooltipTimestamp - hours * 3600) / 60);
            double seconds = tooltipTimestamp - hours * 3600 - minutes * 60;

            ImGui::Text("I-Frame: %02d:%02d:%06.3f", hours, minutes, seconds);
            ImGui::End();
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
    const int numButtons = 5;
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

    ImGui::SameLine(0, spacing);
    if (ImGui::Button("10s>>", ImVec2(buttonWidth, buttonHeight))) {
        double seekTime = std::min(state.totalDuration, state.currentTime + 10.0);
        if (_onSeek) {
            _onSeek(seekTime);
        }
    }

    ImGui::SameLine(0, spacing);
    const char* recordLabel = _isRecording ? "Stop Record" : "Start Record";
    if (ImGui::Button(recordLabel, ImVec2(buttonWidth + 50, buttonHeight))) {
        if (_isRecording) {
            if (_onStopRecording) {
                _onStopRecording();
            }
        } else {
            if (_onStartRecording) {
                _isRecording = _onStartRecording();
            }
        }
    }
}