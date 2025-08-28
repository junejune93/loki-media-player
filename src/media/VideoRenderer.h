#pragma once

#include "../gl_common.h"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include "VideoFrame.h"

class VideoRenderer {
public:
    explicit VideoRenderer(GLFWwindow *win, int width, int height);

    ~VideoRenderer();

    VideoRenderer(VideoRenderer &&other) noexcept;

    VideoRenderer &operator=(VideoRenderer &&other) noexcept;

    VideoRenderer(const VideoRenderer &) = delete;

    VideoRenderer &operator=(const VideoRenderer &) = delete;

    void renderFrame(const VideoFrame &frame) const;

private:
    GLFWwindow *_window{nullptr};
    GLuint _texture{0};
    int _width{0};
    int _height{0};
};
