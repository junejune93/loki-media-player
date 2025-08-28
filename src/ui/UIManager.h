#pragma once

#include <memory>
#include <functional>
#include "ControlPanel.h"
#include "FileSelector.h"
#include "OSDRenderer.h"
#include "OSDState.h"
#include "../core/MediaState.h"
#include <GLFW/glfw3.h>

class UIManager {
public:
    UIManager();

    ~UIManager();

    bool initialize(GLFWwindow *window);

    void newFrame() const;

    void render();

    void shutdown();

    void setOnFileSelected(const std::function<void(const std::string &)> &cb);

    FileSelector *getFileSelector() const { return _fileSelector.get(); }

    // OSD
    void updateOSDData(
        const MediaState &mediaState, 
        const CodecInfo &codecState, 
        const std::string &fileName,
        double temperature = 0.0,
        double humidity = 0.0,
        double acceleration = 0.0,
        const std::string &sensorSource = ""
    );
    
    void updateOSDData(const MediaState &mediaState, const CodecInfo &codecState, const std::string &fileName);

    void handleOSDInput(GLFWwindow *window);

    void setWindowSize(int width, int height);

    // Record
    void setStartRecordingCallback(std::function<bool()> cb) const;
    void setStopRecordingCallback(std::function<void()> cb) const;
    void setRecordingState(bool isRecording) const;

private:
    static void setupStyle();

    bool _initialized = false;
    std::unique_ptr<ControlPanel> _controlPanel;
    std::unique_ptr<FileSelector> _fileSelector;
    std::unique_ptr<OSDRenderer> _osdRenderer;
    std::function<void(const std::string &)> _onFileSelected;

    // OSD
    OSDState _osdState;
    int _windowWidth = 1280;
    int _windowHeight = 780;
};