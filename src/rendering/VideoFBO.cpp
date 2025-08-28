#include <iostream>
#include "../gl_common.h"
#include "VideoFBO.h"

VideoFBO::~VideoFBO() {
    if (_fbo) {
        glDeleteFramebuffers(1, &_fbo);
    }

    if (_texture) {
        glDeleteTextures(1, &_texture);
    }
}

void VideoFBO::create(const int w, const int h) {
    _width = w;
    _height = h;

    glGenFramebuffers(1, &_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

    glGenTextures(1, &_texture);
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, _texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "FBO setup failed!\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VideoFBO::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
}

void VideoFBO::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VideoFBO::updateTexture(const uint8_t *frameData) const {
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height,
                    GL_RGB, GL_UNSIGNED_BYTE, frameData);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::vector<uint8_t> VideoFBO::readPixels(bool flip) const {
    // RGBA
    constexpr int bytesPerPixel = 4;
    const int rowSize = _width * bytesPerPixel;
    std::vector<uint8_t> pixels(_width * _height * bytesPerPixel);

    glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
    glReadPixels(0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (flip) {
        // Flip (vertical)
        std::vector<uint8_t> flippedPixels(_width * _height * bytesPerPixel);
        for (int y = 0; y < _height; y++) {
            const uint8_t* src = pixels.data() + (y * rowSize);
            uint8_t* dst = flippedPixels.data() + ((_height - 1 - y) * rowSize);
            std::memcpy(dst, src, rowSize);
        }
        return flippedPixels;
    }
    return pixels;
}