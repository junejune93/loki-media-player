#pragma once

#include <string>

class ShaderProgram {
public:
    ShaderProgram();
    ~ShaderProgram();

    bool loadVertexFragment(const char* vertSrc, const char* fragSrc);
    void use() const;
    void setUniform1i(const std::string& name, int value) const;
    void drawQuad() const;

    GLuint getProgram() const { return _program; }

private:
    static GLuint compileShader(GLenum type, const char* src);
    void setupQuadGeometry();

    GLuint _program = 0;
    GLuint _vao = 0;
    GLuint _vbo = 0;
    GLuint _ebo = 0;
};