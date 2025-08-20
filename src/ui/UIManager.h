#pragma once

#include <memory>
#include <functional>
#include "ControlPanel.h"
#include "FileSelector.h"
#include <GLFW/glfw3.h>

class UIManager {
public:
    UIManager();
    ~UIManager();

    bool initialize(GLFWwindow* window);
    void newFrame() const;
    void render();
    void shutdown();

    void setOnFileSelected(const std::function<void(const std::string&)>& cb);

    FileSelector* getFileSelector() { return _fileSelector.get(); }

private:
    static void setupStyle();

    bool _initialized = false;
    std::unique_ptr<ControlPanel> _controlPanel;
    std::unique_ptr<FileSelector> _fileSelector;
    std::function<void(const std::string&)> _onFileSelected;
};