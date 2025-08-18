#pragma once

#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <stdexcept>
#include "VideoFrame.h"

class VideoRenderer {
public:
    explicit VideoRenderer(GLFWwindow* win, int width, int height);
    ~VideoRenderer();

    VideoRenderer(VideoRenderer&& other) noexcept;
    VideoRenderer& operator=(VideoRenderer&& other) noexcept;

    VideoRenderer(const VideoRenderer&) = delete;
    VideoRenderer& operator=(const VideoRenderer&) = delete;

    void renderFrame(const VideoFrame& frame);

private:
    GLFWwindow* window{nullptr};
    GLuint texture{0};
    int width{0};
    int height{0};
};
