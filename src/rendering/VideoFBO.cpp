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

void VideoFBO::create(int w, int h) {
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

void VideoFBO::updateTexture(const uint8_t* frameData) const {
    glBindTexture(GL_TEXTURE_2D, _texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height,
                    GL_RGB, GL_UNSIGNED_BYTE, frameData);
    glBindTexture(GL_TEXTURE_2D, 0);
}