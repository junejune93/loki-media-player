#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "../gl_common.h"
#include "RenderContext.h"


RenderContext::RenderContext(GLFWwindow* window) 
    : _renderWindow(window) {
}

bool RenderContext::initialize() {
    if (_initialized) {
        return true;
    }
    
    if (!_renderWindow) {
        std::cerr << "Window is null\n";
        return false;
    }
    
    glfwMakeContextCurrent(_renderWindow);
    
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return false;
    }
    
    // Enable basic OpenGL features
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    _initialized = true;
    return true;
}

void RenderContext::makeCurrent() {
    if (_renderWindow) {
        glfwMakeContextCurrent(_renderWindow);
    }
}

void RenderContext::swapBuffers() {
    if (_renderWindow) {
        glfwSwapBuffers(_renderWindow);
    }
}

void RenderContext::setViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void RenderContext::clearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void RenderContext::clear(GLbitfield mask) {
    glClear(mask);
}