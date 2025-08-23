#pragma once

#include "../gl_common.h"
#include "../core/MediaState.h"
#include "../rendering/VideoFBO.h"
#include "../rendering/ShaderProgram.h"
#include "../threads/AudioThread.h"
#include "../media/interface/IVideoSource.h"
#include "../media/VideoRenderer.h"
#include "../media/AudioPlayer.h"
#include "../media/SyncManager.h"
#include "../media/ThreadSafeQueue.h"
#include "../media/VideoFrame.h"
#include "../media/AudioFrame.h"
#include "../media/Encoder.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

class MediaPlayer {
public:
    MediaPlayer();

    ~MediaPlayer();

    bool initialize(GLFWwindow *window, int videoWidth, int videoHeight);

    bool loadFile(const std::string &filename);

    // Playback
    void play();

    void pause();

    void stop();

    // Seek
    void seek(double time);

    // Record
    bool startRecording(const std::string& outputDir);

    void stopRecording();

    bool isRecording() const { return _isRecording; }
    
    void setOnRecordingStateChanged(std::function<void(bool)> cb);

    // 비디오 프레임 업데이트
    void update();

    // 비디오 렌더링
    void render(int windowWidth, int windowHeight, int controlsHeight);

    const MediaState &getState() const { return _state; }

    MediaState &getState() { return _state; }

    double getDuration() {
        return _source != nullptr ? _source->getDuration() : 0.0;
    }

    CodecInfo getCodecInfo() {
        return _source != nullptr ? _source->getCodecInfo() : CodecInfo{};
    }

private:
    void initializeQueues();

    void swapFrameBuffers();

    bool initializeShaders();

    std::unique_ptr<IVideoSource> _source;
    std::unique_ptr<VideoRenderer> _renderer;
    std::unique_ptr<AudioPlayer> _audioPlayer;
    std::unique_ptr<SyncManager> _syncManager;
    std::unique_ptr<AudioThread> _audioThread;

    std::unique_ptr<VideoFBO> _frontFBO;
    std::unique_ptr<VideoFBO> _backFBO;
    std::unique_ptr<ShaderProgram> _shaderProgram;

    MediaState _state;
    ThreadSafeQueue<VideoFrame> _videoQueue;
    ThreadSafeQueue<AudioFrame> _audioQueue;

    std::chrono::steady_clock::time_point _lastFrameTime;
    static constexpr auto TARGET_FRAME_TIME = std::chrono::milliseconds(16);

    int _videoWidth = 0;
    int _videoHeight = 0;

    // Record
    std::atomic<bool> _isRecording{false};
    std::function<void(bool)> _onRecordingStateChanged;
};