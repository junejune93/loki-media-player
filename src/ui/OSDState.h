#pragma once

#include <string>
#include <chrono>

struct OSDState {
    // OSD - Control
    bool visible = true;
    bool showPlaybackInfo = true;
    bool showStatusInfo = true;
    float fadeAlpha = 1.0f;

    // OSD - Playing Info
    double currentTime = 0.0;
    double totalDuration = 0.0;
    float playbackSpeed = 1.0f;
    float volumeLevel = 1.0f;
    std::string fileName;

    // OSD - Playing State
    bool isPlaying = false;
    bool isBuffering = false;
    std::string syncStatus = "Synced";

    // Interaction
    std::chrono::steady_clock::time_point lastInteraction;

    void updateInteraction() {
        lastInteraction = std::chrono::steady_clock::now();
        fadeAlpha = 1.0f;
    }

    void updateFade() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastInteraction).count();

        if (elapsed > 3000) {
            auto fadeTime = ((float) elapsed - 3000) / 2000.0f;
            fadeAlpha = std::max(0.3f, 1.0f - fadeTime);
        } else {
            fadeAlpha = 1.0f;
        }
    }

    static std::string formatTime(double seconds) {
        int hours = static_cast<int>(seconds) / 3600;
        int minutes = (static_cast<int>(seconds) % 3600) / 60;
        int secs = static_cast<int>(seconds) % 60;

        if (hours > 0) {
            // HH:MM:SS
            char buffer[16] = {0x00,};
            snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, secs);
            return {buffer};
        } else {
            // MM:SS
            char buffer[16] = {0x00,};
            snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
            return {buffer};
        }
    }

    static std::string extractFileName(const std::string &filePath) {
        size_t lastSlash = filePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            return filePath.substr(lastSlash + 1);
        }
        return filePath;
    }
};