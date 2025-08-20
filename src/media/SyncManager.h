#pragma once

#include "VideoFrame.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <algorithm>

class SyncManager {
public:
    SyncManager()
            : _firstVideoPts(-1.0),
              _firstAudioPts(-1.0),
              _dropCount(0),
              _initialized(false),
              _paused(false) {

    }

    void initialize(double videoPts, double audioPts) {
        if (!_initialized) {
            _firstVideoPts = videoPts;
            _firstAudioPts = audioPts;
            _initialized = true;
        }
    }

    void setAudioClock(double audioPts) {
        _audioClock.store(audioPts);
    }

    bool syncVideo(const VideoFrame &vf) {
        if (!_initialized || _paused) {
            return true;
        }

        double videoPts = vf.pts;
        double currentAudioPts = _audioClock.load();

        double ptsOffset = _firstAudioPts - _firstVideoPts;
        videoPts += ptsOffset;

        double diff = videoPts - currentAudioPts;

        const double MAX_DELAY = 0.15;
        const double MIN_DELAY = -0.15;

        if (diff > 0.02 && diff < MAX_DELAY) {
            std::this_thread::sleep_for(std::chrono::duration<double>(std::min(diff, 0.05)));
            return true;
        } else if (diff < MIN_DELAY) {
            _dropCount++;
            if (_dropCount % 5 == 0) {
                std::cerr << "[Sync] Dropping video frame: "
                          << "VideoPTS=" << vf.pts
                          << ", AudioPTS=" << currentAudioPts
                          << ", Diff=" << diff << std::endl;
                return false;
            }
            return true;
        }

        return true;
    }

    bool isInitialized() const { return _initialized; }

    void pause() noexcept { _paused = true; }

    void resume() noexcept { _paused = false; }

    bool isPaused() const noexcept { return _paused; }

    void reset() {
        _firstVideoPts = -1.0;
        _firstAudioPts = -1.0;
        _dropCount = 0;
        _initialized = false;
        _paused = false;
        _audioClock.store(0.0);
    }

private:
    std::atomic<double> _audioClock{0.0};
    double _firstVideoPts;
    double _firstAudioPts;
    int _dropCount;
    bool _initialized;
    bool _paused;
};