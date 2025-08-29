#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include <cstddef>

struct AudioFrame {
    int sampleRate{};
    int channels{};
    int samples{};
    double pts{};
    std::vector<uint8_t> data;

    AudioFrame() = default;

    ~AudioFrame() = default;

    AudioFrame(const AudioFrame& other) = default;

    AudioFrame& operator=(const AudioFrame& other) = default;

    AudioFrame(AudioFrame&& other) noexcept
        : sampleRate(other.sampleRate),
          channels(other.channels),
          samples(other.samples),
          pts(other.pts),
          data(std::move(other.data)) {
    }

    AudioFrame& operator=(AudioFrame&& other) noexcept {
        if (this != &other) {
            sampleRate = other.sampleRate;
            channels = other.channels;
            samples = other.samples;
            pts = other.pts;
            data = std::move(other.data);
        }
        return *this;
    }

    AudioFrame(const int rate, const int ch, const int nbSamples, const double timestamp, std::vector<uint8_t> &&buffer) noexcept
            : sampleRate(rate),
              channels(ch),
              samples(nbSamples),
              pts(timestamp),
              data(std::move(buffer)) {

    }

    int totalSamples() const noexcept {
        return samples * channels;
    }

    size_t dataSize() const noexcept {
        return data.size() * sizeof(uint8_t);
    }

    void reset() {
        sampleRate = 0;
        channels = 0;
        samples = 0;
        pts = 0.0;
        data.clear();
        data.shrink_to_fit();
    }
};