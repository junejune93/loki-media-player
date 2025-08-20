#pragma once

#include <string>
#include <cstdint>

struct CodecInfo {
    std::string videoCodec;
    std::string audioCodec;
    std::string videoResolution;
    std::string videoBitrate;
    std::string audioSampleRate;
    std::string audioBitrate;
    std::string audioChannels;
    std::string containerFormat;

    bool hasVideo = false;
    bool hasAudio = false;

    void clear() {
        videoCodec.clear();
        audioCodec.clear();
        videoResolution.clear();
        videoBitrate.clear();
        audioSampleRate.clear();
        audioBitrate.clear();
        audioChannels.clear();
        containerFormat.clear();
        hasVideo = false;
        hasAudio = false;
    }

    bool isEmpty() const {
        return !hasVideo && !hasAudio;
    }

    static std::string formatBitrate(int64_t bitrate) {
        if (bitrate <= 0) return "Unknown";

        if (bitrate >= 1000000) {
            return std::to_string(bitrate / 1000000) + " Mbps";
        } else if (bitrate >= 1000) {
            return std::to_string(bitrate / 1000) + " kbps";
        } else {
            return std::to_string(bitrate) + " bps";
        }
    }

    static std::string formatChannelLayout(int channels, uint64_t channel_layout) {
        switch (channels) {
            case 1:
                return "Mono";
            case 2:
                return "Stereo";
            case 6:
                return "5.1";
            case 8:
                return "7.1";
            default:
                return std::to_string(channels) + " channels";
        }
    }

    static std::string formatSampleRate(int sampleRate) {
        if (sampleRate >= 1000) {
            return std::to_string(sampleRate / 1000) + " kHz";
        } else {
            return std::to_string(sampleRate) + " Hz";
        }
    }
};