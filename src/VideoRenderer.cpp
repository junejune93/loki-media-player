#include "VideoRenderer.h"
#include <stdexcept>

VideoRenderer::VideoRenderer(GLFWwindow* win, int w, int h)
    : window(win),
    width(w),
    height(h) {
    if (!window) {
        throw std::invalid_argument("GLFWwindow pointer is null");
    }

    glGenTextures(1, &texture);
    if (texture == 0) {
        throw std::runtime_error("Failed to generate OpenGL texture");
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);

    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 1.f);
}

VideoRenderer::~VideoRenderer() {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
    }
}

VideoRenderer::VideoRenderer(VideoRenderer&& other) noexcept
    : window(other.window),
    texture(other.texture),
    width(other.width),
    height(other.height) {
    other.texture = 0;
    other.window = nullptr;
}

VideoRenderer& VideoRenderer::operator=(VideoRenderer&& other) noexcept {
    if (this != &other) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }

        window = other.window;
        texture = other.texture;
        width = other.width;
        height = other.height;

        other.texture = 0;
        other.window = nullptr;
    }
    return *this;
}

void VideoRenderer::renderFrame(const VideoFrame& frame) {
    if (frame.data.empty()) {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 frame.width, frame.height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, frame.data.data());

    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_QUADS);
        glTexCoord2f(0.f, 1.f); glVertex2f(-1.f, -1.f);
        glTexCoord2f(1.f, 1.f); glVertex2f( 1.f, -1.f);
        glTexCoord2f(1.f, 0.f); glVertex2f( 1.f,  1.f);
        glTexCoord2f(0.f, 0.f); glVertex2f(-1.f,  1.f);
    glEnd();

    glfwSwapBuffers(window);
}