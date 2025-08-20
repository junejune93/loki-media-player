#pragma once

#include <string>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>

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

    void updateVolumeFromSystem() {
        float newVolume = getSystemVolume();
        if (newVolume >= 0.0f) {
            volumeLevel = newVolume;
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

private:
    static float getPulseAudioVolume() {
        try {
            std::string result = executeCommand("pactl get-sink-volume @DEFAULT_SINK@ 2>/dev/null");

            size_t percentPos = result.find('%');
            if (percentPos != std::string::npos) {
                size_t start = percentPos;
                while (start > 0 && (result[start] != ' ' && result[start] != '\t')) {
                    start--;
                }
                if (result[start] == ' ' || result[start] == '\t') {
                    start++;
                }

                std::string percentStr = result.substr(start, percentPos - start);

                std::string cleanPercent;
                for (char c : percentStr) {
                    if (std::isdigit(c)) {
                        cleanPercent += c;
                    }
                }

                if (!cleanPercent.empty()) {
                    return std::stof(cleanPercent) / 100.0f;
                }
            }
        } catch (const std::exception& e) {
        }
        return -1.0f;
    }

    static float getALSAVolume() {
        try {
            std::string result = executeCommand("amixer get Master 2>/dev/null");

            size_t start = result.find('[');
            size_t end = result.find('%', start);

            if (start != std::string::npos && end != std::string::npos) {
                std::string percentStr = result.substr(start + 1, end - start - 1);
                return std::stof(percentStr) / 100.0f;
            }
        } catch (const std::exception& e) {
        }
        return -1.0f;
    }

    static float getSystemVolume() {
        float volume = getPulseAudioVolume();

        if (volume < 0.0f) {
            volume = getALSAVolume();
        }

        if (volume >= 0.0f) {
            volume = std::max(0.0f, std::min(1.0f, volume));
        }

        return volume;
    }

    static std::string executeCommand(const std::string& command) {
        std::array<char, 128> buffer{};
        std::string result;

        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }

        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        return result;
    }
};