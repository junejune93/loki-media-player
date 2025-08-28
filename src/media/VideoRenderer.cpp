#include "VideoRenderer.h"
#include <stdexcept>

VideoRenderer::VideoRenderer(GLFWwindow *win, int w, int h)
        : _window(win),
          _width(w),
          _height(h) {
    if (!_window) {
        throw std::invalid_argument("GLFWwindow pointer is null");
    }

    glGenTextures(1, &_texture);
    if (_texture == 0) {
        throw std::runtime_error("Failed to generate OpenGL _texture");
    }

    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);

    glViewport(0, 0, _width, _height);
    glClearColor(0.f, 0.f, 0.f, 1.f);
}

VideoRenderer::~VideoRenderer() {
    if (_texture != 0) {
        glDeleteTextures(1, &_texture);
    }
}

VideoRenderer::VideoRenderer(VideoRenderer &&other) noexcept
        : _window(other._window),
          _texture(other._texture),
          _width(other._width),
          _height(other._height) {
    other._texture = 0;
    other._window = nullptr;
}

VideoRenderer &VideoRenderer::operator=(VideoRenderer &&other) noexcept {
    if (this != &other) {
        if (_texture != 0) {
            glDeleteTextures(1, &_texture);
        }

        _window = other._window;
        _texture = other._texture;
        _width = other._width;
        _height = other._height;

        other._texture = 0;
        other._window = nullptr;
    }
    return *this;
}

void VideoRenderer::renderFrame(const VideoFrame &frame) const {
    if (frame.data.empty()) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 frame.width, frame.height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, frame.data.data());

    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_QUADS);
    glTexCoord2f(0.f, 1.f);
    glVertex2f(-1.f, -1.f);
    glTexCoord2f(1.f, 1.f);
    glVertex2f(1.f, -1.f);
    glTexCoord2f(1.f, 0.f);
    glVertex2f(1.f, 1.f);
    glTexCoord2f(0.f, 0.f);
    glVertex2f(-1.f, 1.f);
    glEnd();

    glfwSwapBuffers(_window);
}