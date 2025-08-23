#include <iostream>
#include <filesystem>
#include "../gl_common.h"
#include "../media/FileVideoSource.h"
#include "MediaPlayer.h"
#include "Utils.h"

namespace fs = std::filesystem;

MediaPlayer::MediaPlayer() = default;

MediaPlayer::~MediaPlayer() {
    stop();
}

bool MediaPlayer::initialize(GLFWwindow *window, int videoWidth, int videoHeight) {
    _videoWidth = videoWidth;
    _videoHeight = videoHeight;

    // Initialize audio player
    _audioPlayer = std::make_unique<AudioPlayer>(48000, 2);
    _syncManager = std::make_unique<SyncManager>();

    // Initialize renderer
    _renderer = std::make_unique<VideoRenderer>(window, videoWidth, videoHeight);

    // Initialize FBOs
    _frontFBO = std::make_unique<VideoFBO>();
    _backFBO = std::make_unique<VideoFBO>();
    _frontFBO->create(videoWidth, videoHeight);
    _backFBO->create(videoWidth, videoHeight);

    // Initialize shaders
    if (!initializeShaders()) {
        std::cerr << "Failed to initialize shaders\n";
        return false;
    }

    // Initialize frame time
    _lastFrameTime = std::chrono::steady_clock::now();

    return true;
}

bool MediaPlayer::initializeShaders() {
    _shaderProgram = std::make_unique<ShaderProgram>();

    const char *vertexShaderSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTex;
        out vec2 TexCoord;
        void main() {
            TexCoord = aTex;
            gl_Position = vec4(aPos.xy, 0.0, 1.0);
        })";

    const char *fragmentShaderSrc = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D screenTexture;
        void main() { FragColor = texture(screenTexture, TexCoord); })";

    return _shaderProgram->loadVertexFragment(vertexShaderSrc, fragmentShaderSrc);
}

bool MediaPlayer::loadFile(const std::string &filename) {
    try {
        // Clean up existing decoder
        if (_source) {
            stop();
            _audioThread.reset();
            _source.reset();
        }

        _source = std::make_unique<FileVideoSource>(filename);
        _state.currentFile = filename;
        _state.totalDuration = _source->getDuration();
        _state.reset();

        // Initialize audio thread
        _audioThread = std::make_unique<AudioThread>(
                _source->getAudioQueue(),
                _source->getVideoQueue(),
                *_audioPlayer,
                *_syncManager,
                *_source
        );

        _audioThread->start();
        return true;

    } catch (const std::exception &e) {
        std::cerr << "Failed to load file: " << e.what() << "\n";
        return false;
    }
}

void MediaPlayer::play() {
    if (!_source) return;

    if (!_state.isPlaying) {
        _source->stop();
        _source->getVideoQueue().clear();
        _source->getAudioQueue().clear();
        _syncManager->reset();

        _source->start();
        _state.isPlaying = true;
        _state.isPaused = false;

        if (_audioThread) {
            _audioThread->setPlaying(true);
        }
        _syncManager->resume();
    }
}

void MediaPlayer::pause() {
    if (_state.isPlaying) {
        _state.isPlaying = false;
        _state.isPaused = true;

        if (_audioThread) {
            _audioThread->setPlaying(false);
        }
        _syncManager->pause();
    }
}

void MediaPlayer::stop() {
    _state.isPlaying = false;
    _state.isPaused = false;

    if (_source) {
        _source->stop();
        _source->getVideoQueue().clear();
        _source->getAudioQueue().clear();
    }

    if (_audioThread) {
        _audioThread->setPlaying(false);
    }

    if (_syncManager) {
        _syncManager->reset();
    }

    _state.currentTime = 0.0;
}

void MediaPlayer::seek(double time) {
    _state.seekTarget = time;
    _state.seekRequested = true;

    if (_audioThread) {
        _audioThread->requestSeek(time);
    }
}

void MediaPlayer::update() {
    if (!_source) {
        return;
    }

    if (!_state.isPlaying) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (now - _lastFrameTime >= TARGET_FRAME_TIME) {
        auto &videoQueue = _source->getVideoQueue();

        // 비디오 큐에서 프레임 추출
        if (auto vfOpt = Utils::waitPopOpt(videoQueue, 5)) {
            auto vf = std::move(*vfOpt);
            
            if (!_syncManager->isInitialized()) {
                _syncManager->initialize(vf.pts, vf.pts, 0);
            }

            _syncManager->setAudioClock(vf.pts);
            
            _backFBO->bind();
            try {
                glViewport(0, 0, _videoWidth, _videoHeight);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);

                _renderer->renderFrame(vf);
                swapFrameBuffers();
                _lastFrameTime = now;
                
                _state.currentTime = vf.pts;
                
                // Set Record Queue
                if (_isRecording && _source) {
                    _source->encodeFrame(vf);
                }
            } catch (const std::exception &e) {
                std::cerr << "Error rendering frame: " << e.what() << std::endl;
            }
            _backFBO->unbind();
        }
    }

    // Update current time from audio thread
    if (_audioThread) {
        _state.currentTime = _audioThread->getCurrentTime();
    }
}

void MediaPlayer::render(int windowWidth, int windowHeight, int controlsHeight) {
    // Draw front FBO to screen (비디오를 상단에 표시)
    glViewport(0, controlsHeight, windowWidth, windowHeight - controlsHeight);

    _shaderProgram->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _frontFBO->getTexture());
    _shaderProgram->setUniform1i("screenTexture", 0);

    _shaderProgram->drawQuad();

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    glViewport(0, 0, windowWidth, windowHeight);
}

void MediaPlayer::swapFrameBuffers() {
    std::swap(_frontFBO, _backFBO);
}

bool MediaPlayer::startRecording(const std::string& outputDir) {
    if (_isRecording || !_source) {
        return false;
    }

    try {
        // Record 버튼 변경
        if (_onRecordingStateChanged) {
            _onRecordingStateChanged(true);
        }

        // Record
        _isRecording = true;
        _source->startRecord();

        std::cout << "Recording started. Output directory: " << outputDir << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        _isRecording = false;
        if (_onRecordingStateChanged) {
            _onRecordingStateChanged(false);
        }
        return false;
    }
}

void MediaPlayer::stopRecording() {
    bool wasRecording = _isRecording.exchange(false);
    if (!wasRecording || !_source) {
        return;
    }

    _source->stopRecord();


    // Record 버튼 변경
    if (_onRecordingStateChanged) {
        _onRecordingStateChanged(false);
    }
}

void MediaPlayer::setOnRecordingStateChanged(std::function<void(bool)> cb) {
    _onRecordingStateChanged = std::move(cb);
}