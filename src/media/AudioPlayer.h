#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <mutex>
#include <portaudio.h>
#include <stdexcept>
#include <vector>
#include "AudioFrame.h"

class AudioPlayer {
public:
    AudioPlayer(const int sampleRate, const int channels) : _sampleRate(sampleRate), _channels(channels) {
        if (Pa_Initialize() != paNoError) {
            throw std::runtime_error("Pa_Initialize failed");
        }

        const PaError err = Pa_OpenDefaultStream(&_stream, 0, channels, paInt16, sampleRate, 1024,
                                                 &AudioPlayer::audioCallback, this);
        if (err != paNoError) {
            throw std::runtime_error("Pa_OpenDefaultStream failed");
        }

        if (Pa_StartStream(_stream) != paNoError) {
            throw std::runtime_error("Pa_StartStream failed");
        }
    }

    ~AudioPlayer() noexcept {
        if (_stream) {
            Pa_StopStream(_stream);
            Pa_CloseStream(_stream);
        }
        Pa_Terminate();
    }

    void queueFrame(AudioFrame &&frame) {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        _audioBuffer.emplace_back(std::move(frame));
    }

    void queueFrame(const AudioFrame &frame) {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        _audioBuffer.push_back(frame);
    }

    double getCurrentPts() const noexcept {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        double pts = _lastPlayedPts;

        size_t bufferedSamples = 0;
        for (const auto &frame: _audioBuffer) {
            bufferedSamples += frame.data.size() / sizeof(int16_t) / _channels;
        }

        pts += static_cast<double>(bufferedSamples) / _sampleRate;
        return pts;
    }

    void pause() const noexcept {
        if (_stream) {
            Pa_StopStream(_stream);
        }
    }

    void resume() const noexcept {
        if (_stream) {
            Pa_StartStream(_stream);
        }
    }

private:
    static int audioCallback(const void * /*inputBuffer*/, void *outputBuffer, const unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo * /*timeInfo*/, PaStreamCallbackFlags /*statusFlags*/,
                             void *userData) noexcept {
        auto *player = static_cast<AudioPlayer *>(userData);
        return player->processAudio(outputBuffer, framesPerBuffer);
    }

    int processAudio(void *outputBuffer, const unsigned long framesPerBuffer) noexcept {
        auto *out = static_cast<int16_t *>(outputBuffer);
        const size_t samplesNeeded = framesPerBuffer * _channels;
        size_t samplesWritten = 0;

        std::lock_guard<std::mutex> lock(_bufferMutex);

        while (samplesWritten < samplesNeeded && !_audioBuffer.empty()) {
            auto &frame = _audioBuffer.front();
            const auto *frameData = reinterpret_cast<const int16_t *>(frame.data.data());
            const size_t frameSamples = frame.data.size() / sizeof(int16_t);
            const size_t samplesToTake = std::min(samplesNeeded - samplesWritten, frameSamples - _currentFrameOffset);

            std::copy_n(frameData + _currentFrameOffset, samplesToTake, out + samplesWritten);

            samplesWritten += samplesToTake;
            _currentFrameOffset += samplesToTake;

            if (_currentFrameOffset >= frameSamples) {
                _lastPlayedPts = frame.pts;
                _audioBuffer.pop_front();
                _currentFrameOffset = 0;
            }
        }

        if (samplesWritten < samplesNeeded) {
            std::fill(out + samplesWritten, out + samplesNeeded, 0);
        }

        return paContinue;
    }

private:
    int _sampleRate;
    int _channels;
    PaStream *_stream{nullptr};

    std::deque<AudioFrame> _audioBuffer;
    mutable std::mutex _bufferMutex;
    size_t _currentFrameOffset{0};
    double _lastPlayedPts{0.0};
};