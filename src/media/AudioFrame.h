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
};