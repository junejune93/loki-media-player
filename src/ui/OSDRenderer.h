#pragma once

#include "OSDState.h"
#include <GLFW/glfw3.h>

class OSDRenderer {
public:
    OSDRenderer();

    ~OSDRenderer();

    // OSD rendering
    void render(const OSDState &state, int windowWidth, int windowHeight);

    // Key Input
    void handleInput(GLFWwindow *window, OSDState &state);

private:
    static void renderPlaybackInfo(const OSDState &state, int windowWidth);

    static void renderCodecInfo(const OSDState &state, int windowWidth, int windowHeight);

    static void renderSyncInfo(const OSDState &state, int windowWidth, int windowHeight);

    static void renderSensorInfo(const OSDState &state, int windowWidth, int windowHeight);

    void renderCenterStatus(const OSDState &state, int windowWidth, int windowHeight);

    static void setupOSDStyle(float alpha);

    static void restoreOSDStyle();

    bool _oKeyPressed = false;      // Key: O (OSD Toggle)
    bool _iKeyPressed = false;      // Key: I (Play Info Toggle)
    bool _sKeyPressed = false;      // Key: S (Play State Toggle)
    bool _cKeyPressed = false;      // Key: C (Codec Info Toggle)
    bool _kKeyPressed = false;      // Key: K (Sensor Info Toggle)

    double _lastMouseX = 0.0;
    double _lastMouseY = 0.0;

    std::chrono::steady_clock::time_point _lastStateChange;
    bool _lastPlayingState = false;
};