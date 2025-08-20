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

private:
    void renderProgressBar(MediaState &state);

    void renderControlButtons(const MediaState &state);

    static void renderTimeDisplay(const MediaState &state);

    int _videoWidth;
    int _controlsHeight;

    std::function<void()> _onPlay;
    std::function<void()> _onPause;
    std::function<void()> _onStop;
    std::function<void(double)> _onSeek;
};