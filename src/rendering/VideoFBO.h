#pragma once

#include "../gl_common.h"
#include <cstdint>

class VideoFBO {
public:
    VideoFBO() = default;

    ~VideoFBO();

    void create(int w, int h);

    void bind() const;

    static void unbind();

    void updateTexture(const uint8_t *frameData) const;

    GLuint getTexture() const { return _texture; }

    int getWidth() const { return _width; }

    int getHeight() const { return _height; }

private:
    GLuint _fbo = 0;
    GLuint _texture = 0;
    int _width = 0;
    int _height = 0;
};