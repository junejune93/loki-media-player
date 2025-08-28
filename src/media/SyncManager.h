#pragma once

#include "VideoFrame.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <limits>
#include <iomanip>

constexpr double MAX_INTER_CHANNEL_SYNC_MS = 0.002;
constexpr double MAX_AUDIO_VIDEO_SYNC_MS = 0.01;
constexpr size_t MAX_FRAME_QUEUE_SIZE = 3;

class SyncManager {
public:
    using ChannelId = size_t;

    struct SyncedFrame {
        VideoFrame frame;
        double pts;
        ChannelId channelId;
        std::chrono::high_resolution_clock::time_point arrivalTime;
    };

    explicit SyncManager(size_t numChannels = 4)
            : _numChannels(numChannels),
              _masterChannel(0),
              _firstVideoPts(-1.0),
              _firstAudioPts(-1.0),
              _dropCount(0),
              _initialized(false),
              _paused(false),
              _running(true),
              _syncThread(&SyncManager::syncLoop, this) {
        _frameQueues.resize(numChannels);
    }

    ~SyncManager() {
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _running = false;
            _cv.notify_all();
        }

        if (_syncThread.joinable()) {
            _syncThread.join();
        }
    }

    void initialize(double videoPts, double audioPts, ChannelId channelId = 0) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_initialized) {
            _firstVideoPts = videoPts;
            _firstAudioPts = audioPts;
            _initialized = true;
            _masterChannel = channelId;
            _cv.notify_all();
        }
    }

    void setAudioClock(double audioPts) {
        std::lock_guard<std::mutex> lock(_mutex);
        _audioClock = audioPts;
        _cv.notify_all();
    }

    bool addFrame(VideoFrame frame, ChannelId channelId) {
        if (channelId >= _numChannels) {
            return false;
        }

        SyncedFrame syncedFrame{
                .frame = std::move(frame),
                .pts = frame.pts,
                .channelId = channelId,
                .arrivalTime = std::chrono::high_resolution_clock::now()
        };

        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_frameQueues[channelId].size() >= MAX_FRAME_QUEUE_SIZE) {
                // 오래된 비디오 프레임은 드랍 처리
                _frameQueues[channelId].pop_front();
                _dropCount++;
                if (_dropCount % 10 == 0) {
                    std::cerr << "[Sync] Dropped frame from channel " << channelId
                              << " (queue full)" << std::endl;
                }
            }
            _frameQueues[channelId].push_back(std::move(syncedFrame));
            _cv.notify_all();
        }
        return true;
    }

    std::vector<VideoFrame> getSynchronizedFrames() {
        std::vector<VideoFrame> frames(_numChannels);
        std::unique_lock<std::mutex> lock(_mutex);

        _cv.wait(lock, [this] {
            if (!_running) {
                return true;
            }
            if (!_initialized || _paused) {
                return false;
            }

            for (const auto &queue: _frameQueues) {
                if (queue.empty()) {
                    return false;
                }
            }
            return true;
        });

        if (!_running) return {};
        if (_paused) return {};

        // 마스터 채널의 PTS 획득
        if (_frameQueues[_masterChannel].empty()) return {};
        const double refPts = _frameQueues[_masterChannel].front().pts;

        // 각 채널에서 마스터 채널의 PTS에 가장 가까운 프레임 획득
        for (size_t i = 0; i < _numChannels; ++i) {
            if (i == _masterChannel) {
                frames[i] = std::move(_frameQueues[i].front().frame);
                _frameQueues[i].pop_front();
                continue;
            }

            auto &queue = _frameQueues[i];
            if (queue.empty()) {
                continue;
            }

            // 가장 가까운 PTS에 있는 프레임 찾기
            auto bestMatch = queue.begin();
            double minDiff = std::abs(bestMatch->pts - refPts);
            for (auto it = queue.begin(); it != queue.end(); ++it) {
                double diff = std::abs(it->pts - refPts);
                if (diff < minDiff) {
                    minDiff = diff;
                    bestMatch = it;
                }
            }

            // 허용 오차 내에서 최적 매치 탐색
            if (minDiff <= MAX_INTER_CHANNEL_SYNC_MS) {
                // 최적 매치 된 프레임 설정하고 그 외 프레임은 제거
                frames[i] = std::move(bestMatch->frame);
                queue.erase(queue.begin(), std::next(bestMatch));
            } else if (minDiff > MAX_INTER_CHANNEL_SYNC_MS * 2) {
                // 동기화 실패하면 프레임 드랍 처리
                if (!queue.empty()) {
                    queue.pop_front();
                }
                _dropCount++;
                if (_dropCount % 10 == 0) {
                    std::cerr << "[Sync] Dropped frame from channel " << i
                              << " (out of sync: " << (minDiff * 1000.0) << "ms > "
                              << (MAX_INTER_CHANNEL_SYNC_MS * 1000.0) << "ms)" << std::endl;
                }
            }
        }

        return frames;
    }

    size_t getQueueSize(ChannelId channelId) const {
        std::lock_guard<std::mutex> lock(_mutex);
        if (channelId >= _frameQueues.size()) return 0;
        return _frameQueues[channelId].size();
    }

    size_t getDropCount() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _dropCount;
    }

    bool isInitialized() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _initialized;
    }

    void pause() noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        _paused = true;
        _cv.notify_all();
    }

    void resume() noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        _paused = false;
        _cv.notify_all();
    }

    bool isPaused() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _paused;
    }

    void reset() {
        std::unique_lock<std::mutex> lock(_mutex);
        _firstVideoPts = -1.0;
        _firstAudioPts = -1.0;
        _dropCount = 0;
        _initialized = false;
        _paused = false;
        _audioClock = 0.0;

        for (auto &queue: _frameQueues) {
            queue.clear();
        }

        _cv.notify_all();
    }

private:
    void syncLoop() {
        while (_running) {
            auto frames = getSynchronizedFrames();
            if (!_running) {
                break;
            }

            static auto lastLogTime = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime).count() >= 5) {
                logSyncStatus();
                lastLogTime = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void logSyncStatus() const {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_initialized || _frameQueues.empty()) {
            return;
        }

        std::cout << "\n[Sync Status]" << std::endl;
        std::cout << "Master Channel: " << _masterChannel << std::endl;
        std::cout << "Dropped Frames: " << _dropCount << std::endl;

        if (!_frameQueues[_masterChannel].empty()) {
            double refPts = _frameQueues[_masterChannel].front().pts;
            std::cout << "Reference PTS: " << std::fixed << std::setprecision(6)
                      << refPts << "s" << std::endl;

            for (size_t i = 0; i < _numChannels; ++i) {
                if (_frameQueues[i].empty()) {
                    continue;
                }

                double pts = _frameQueues[i].front().pts;
                double diffMs = (pts - refPts) * 1000.0;

                std::cout << "Channel " << i << ": "
                          << _frameQueues[i].size() << " frames, "
                          << "PTS: " << std::fixed << std::setprecision(6) << pts << "s, "
                          << "Diff: " << std::setprecision(3) << diffMs << "ms "
                          << (i == _masterChannel ? "(master)" : "")
                          << (std::abs(diffMs) > MAX_INTER_CHANNEL_SYNC_MS * 1000.0 ? " [OUT OF SYNC]" : "")
                          << std::endl;
            }
        }
        std::cout << std::endl;
    }

private:
    const size_t _numChannels;
    size_t _masterChannel;

    // 각 채널 별 동기화 된 프레임을 저장하는 큐
    std::vector<std::deque<SyncedFrame>> _frameQueues;

    mutable std::mutex _mutex;
    std::condition_variable _cv;

    double _firstVideoPts;
    double _firstAudioPts;
    double _audioClock{0.0};
    size_t _dropCount{0};
    bool _initialized;
    bool _paused;
    bool _running{true};

    std::thread _syncThread;
};