#include "AudioThread.h"
#include "../core/Utils.h"

AudioThread::AudioThread(ThreadSafeQueue<AudioFrame> &audioQueue,
                         ThreadSafeQueue<VideoFrame> &videoQueue,
                         AudioPlayer &audioPlayer,
                         SyncManager &syncManager,
                         IVideoSource &source)
        : _audioQueue(audioQueue), _videoQueue(videoQueue), _audioPlayer(audioPlayer), _syncManager(syncManager),
          _source(source) {
}

AudioThread::~AudioThread() {
    stop();
}

void AudioThread::start() {
    if (!_running) {
        _running = true;
        _thread = std::thread(&AudioThread::run, this);
    }
}

void AudioThread::stop() {
    _running = false;
    if (_thread.joinable()) {
        _thread.join();
    }
}

void AudioThread::requestSeek(double time) {
    _seekTarget = time;
    _seekRequested = true;
}

void AudioThread::run() {
    while (_running) {
        if (_playing) {
            // Handle seek requests
            if (_seekRequested) {
                _source.seek(_seekTarget);
                _syncManager.reset();
                _videoQueue.clear();
                _audioQueue.clear();
                _seekRequested = false;
                continue;
            }

            if (auto afOpt = Utils::waitPopOpt(_audioQueue, 10)) {
                auto &af = *afOpt;
                _audioPlayer.queueFrame(af);
                _syncManager.setAudioClock(_audioPlayer.getCurrentPts());
                _currentTime = _audioPlayer.getCurrentPts();

                // Initialize sync manager if needed
                if (!_syncManager.isInitialized() && !_videoQueue.empty()) {
                    if (auto vfOpt = Utils::waitPopOpt(_videoQueue, 5)) {
                        if (vfOpt->pts >= 0 && af.pts >= 0) {
                            _syncManager.initialize(vfOpt->pts, af.pts);
                        }
                        _videoQueue.push_front(std::move(*vfOpt));
                    }
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}