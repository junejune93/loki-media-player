#pragma once

#include "../gl_common.h"
#include <GLFW/glfw3.h>

class RenderContext {
public:
    explicit RenderContext(GLFWwindow *window);

    ~RenderContext() = default;

    bool initialize();

    void makeCurrent() const;

    void swapBuffers() const;

    static void setViewport(int x, int y, int width, int height);

    static void clearColor(float r, float g, float b, float a = 1.0f);

    static void clear(GLbitfield mask = GL_COLOR_BUFFER_BIT);

    GLFWwindow *getWindow() const { return _renderWindow; }

private:
    GLFWwindow *_renderWindow;
    bool _initialized = false;
};