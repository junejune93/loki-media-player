#pragma once

#include <vector>
#include <cstdint>
#include <utility>

struct VideoFrame {
    int width{0};
    int height{0};
    double pts{0.0};
    std::vector<uint8_t> data;

    VideoFrame() = default;

    VideoFrame(VideoFrame &&) noexcept = default;

    VideoFrame &operator=(VideoFrame &&) noexcept = default;

    VideoFrame(const VideoFrame &) = default;

    VideoFrame &operator=(const VideoFrame &) = default;

    VideoFrame(const int w, const int h, const double t, std::vector<uint8_t> &&d)
            : width(w),
              height(h),
              pts(t),
              data(std::move(d)) {

    }
};