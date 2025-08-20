#pragma once

#include <string>

struct MediaState {
    bool isPlaying = false;
    bool isPaused = false;
    bool seekRequested = false;
    double currentTime = 0.0;
    double totalDuration = 0.0;
    double seekTarget = 0.0;
    std::string currentFile;

    double getProgress() const {
        return (totalDuration > 0) ? currentTime / totalDuration : 0.0;
    }

    void requestSeek(double time) {
        seekTarget = time;
        seekRequested = true;
    }

    void reset() {
        isPlaying = false;
        isPaused = false;
        seekRequested = false;
        currentTime = 0.0;
        seekTarget = 0.0;
    }
};