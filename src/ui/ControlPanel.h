#pragma once

#include "core/MediaState.h"
#include "imgui.h"
#include <functional>
#include <utility>

class ControlPanel {
public:
    ControlPanel(int videoWidth, int controlsHeight);

    void render(MediaState &state);

    void setPlayCallback(std::function<void()> cb) { _onPlay = std::move(cb); }

    void setPauseCallback(std::function<void()> cb) { _onPause = std::move(cb); }

    void setStopCallback(std::function<void()> cb) { _onStop = std::move(cb); }

    void setSeekCallback(std::function<void(double)> cb) { _onSeek = std::move(cb); }

    void setWindowSize(int width, int height) {
        _videoWidth = width;
        _controlsHeight = height - (width * 720 / 1280);
    }

    void setStartRecordingCallback(std::function<bool()> cb) { _onStartRecording = std::move(cb); }
    void setStopRecordingCallback(std::function<void()> cb) { _onStopRecording = std::move(cb); }
    void setRecordingState(bool isRecording) { _isRecording = isRecording; }

private:
    void renderProgressBar(MediaState &state);

    void renderControlButtons(const MediaState &state);

    static void renderTimeDisplay(const MediaState &state);

    int _videoWidth;
    int _controlsHeight;

    bool _isRecording{false};
    bool _showMarkers{false};

    std::function<void()> _onPlay;
    std::function<void()> _onPause;
    std::function<void()> _onStop;
    std::function<void(double)> _onSeek;
    std::function<bool()> _onStartRecording;
    std::function<void()> _onStopRecording;
};