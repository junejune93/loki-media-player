#pragma once

#include <string>
#include <vector>

struct MediaState {
    bool isPlaying = false;
    bool isPaused = false;
    bool seekRequested = false;
    double currentTime = 0.0;
    double totalDuration = 0.0;
    double seekTarget = 0.0;
    std::string currentFile;
    double duration = 0.0;
    float playbackSpeed = 1.0f;
    float volume = 1.0f;
    double audioVideoSyncOffset = 0.0;
    std::vector<double> iFrameTimestamps;
    std::vector<double> pFrameTimestamps;

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
        iFrameTimestamps.clear();
        pFrameTimestamps.clear();
    }

    void setIFrameTimestamps(const std::vector<double> &timestamps) { iFrameTimestamps = timestamps; }

    const std::vector<double> &getIFrameTimestamps() const { return iFrameTimestamps; }

    void setPFrameTimestamps(const std::vector<double> &timestamps) { pFrameTimestamps = timestamps; }

    const std::vector<double> &getPFrameTimestamps() const { return pFrameTimestamps; }
};