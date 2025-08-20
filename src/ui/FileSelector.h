#pragma once

#include <utility>
#include <vector>
#include <string>
#include <functional>
#include "imgui.h"

class FileSelector {
public:
    FileSelector() = default;
    ~FileSelector() = default;

    bool isVisible() const {
        return _visible;
    }

    void setVisible(bool v) {
        _visible = v;
    }

    void setFiles(const std::vector<std::string>& files) {
        _files = files;
    }

    void setSelectCallback(std::function<void(const std::string&)> cb) {
        _selectCallback = std::move(cb);
    }

    void render() {
        if (!_visible) {
            return;
        }

        ImGui::Begin("Select Video File", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

        for (const auto& file : _files) {
            if (ImGui::Selectable(file.c_str())) {
                if (_selectCallback) {
                    _selectCallback(file);
                }
                _visible = false;
            }
        }

        ImGui::End();
    }

private:
    bool _visible = false;
    std::vector<std::string> _files;
    std::function<void(const std::string&)> _selectCallback;
};