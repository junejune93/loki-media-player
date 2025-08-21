#pragma once

#include <thread>
#include <atomic>
#include <chrono>
#include "../media/ThreadSafeQueue.h"
#include "../media/AudioFrame.h"
#include "../media/AudioPlayer.h"
#include "../media/SyncManager.h"
#include "media/interface/IVideoSource.h"

struct VideoFrame;

class AudioThread {
public:
    AudioThread(ThreadSafeQueue<AudioFrame> &audioQueue,
                ThreadSafeQueue<VideoFrame> &videoQueue,
                AudioPlayer &audioPlayer,
                SyncManager &syncManager,
                IVideoSource &source);

    ~AudioThread();

    void start();

    void stop();

    void setPlaying(bool playing) { _playing = playing; }

    void requestSeek(double time);

    double getCurrentTime() const { return _currentTime; }

private:
    void run();

    std::thread _thread;
    std::atomic<bool> _running{false};
    std::atomic<bool> _playing{false};
    std::atomic<bool> _seekRequested{false};
    std::atomic<double> _seekTarget{0.0};
    std::atomic<double> _currentTime{0.0};

    ThreadSafeQueue<AudioFrame> &_audioQueue;
    ThreadSafeQueue<VideoFrame> &_videoQueue;
    AudioPlayer &_audioPlayer;
    SyncManager &_syncManager;
    IVideoSource &_source;
};