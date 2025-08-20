#include <iostream>
#include "../gl_common.h"
#include "ShaderProgram.h"

ShaderProgram::ShaderProgram() = default;

ShaderProgram::~ShaderProgram() {
    if (_program) {
        glDeleteProgram(_program);
    }

    if (_vao) {
        glDeleteVertexArrays(1, &_vao);
    }

    if (_ebo) {
        glDeleteBuffers(1, &_ebo);
    }

    if (_ebo) {
        glDeleteBuffers(1, &_ebo);
    }
}

GLuint ShaderProgram::compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << "\n";
    }
    return shader;
}

bool ShaderProgram::loadVertexFragment(const char* vertSrc, const char* fragSrc) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    _program = glCreateProgram();
    glAttachShader(_program, vertexShader);
    glAttachShader(_program, fragmentShader);
    glLinkProgram(_program);

    int success;
    char infoLog[512];
    glGetProgramiv(_program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(_program, 512, nullptr, infoLog);
        std::cerr << "Shader linking error: " << infoLog << "\n";
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    setupQuadGeometry();
    return true;
}

void ShaderProgram::setupQuadGeometry() {
    float vertices[] = {
            -1.f, -1.f, 0.f, 0.f,
            1.f, -1.f, 1.f, 0.f,
            1.f,  1.f, 1.f, 1.f,
            -1.f,  1.f, 0.f, 1.f
    };
    unsigned int indices[] = {0,1,2, 2,3,0};

    glGenVertexArrays(1, &_vao);
    glGenBuffers(1, &_vbo);
    glGenBuffers(1, &_ebo);

    glBindVertexArray(_vao);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void ShaderProgram::use() const {
    glUseProgram(_program);
}

void ShaderProgram::setUniform1i(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(_program, name.c_str()), value);
}

void ShaderProgram::drawQuad() const {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}