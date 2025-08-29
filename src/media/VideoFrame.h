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

    ~VideoFrame() = default;

    VideoFrame(VideoFrame &&other) noexcept
        : width(other.width),
          height(other.height),
          pts(other.pts),
          data(std::move(other.data)) {
    }

    VideoFrame &operator=(VideoFrame &&other) noexcept {
        if (this != &other) {
            width = other.width;
            height = other.height;
            pts = other.pts;
            data = std::move(other.data);
        }
        return *this;
    }

    VideoFrame(const VideoFrame &other) = default;

    VideoFrame &operator=(const VideoFrame &other) = default;

    VideoFrame(const int w, const int h, const double t, std::vector<uint8_t> &&d)
            : width(w),
              height(h),
              pts(t),
              data(std::move(d)) {
    }

    void reset() {
        width = 0;
        height = 0;
        pts = 0.0;
        data.clear();
        data.shrink_to_fit();
    }
};