#pragma once

#include <string>
#include <vector>
#include "media/AudioFrame.h"
#include "media/ThreadSafeQueue.h"
#include "media/VideoFrame.h"
#include "ui/OSDState.h"

class IDecoderSource {
public:
    enum class DecoderType {
        NONE,
        SW
    };

    struct DecoderConfig {
        DecoderType decoderType{DecoderType::SW};
        std::string hwDevice;
        bool enableLowLatency{false};
        int maxThreads{0};
    };

public:
    virtual ~IDecoderSource() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual void flush() = 0;

    virtual bool seek(double timeInSeconds) = 0;

    virtual double getDuration() const = 0;

    virtual ThreadSafeQueue<VideoFrame> &getVideoQueue() = 0;

    virtual ThreadSafeQueue<AudioFrame> &getAudioQueue() = 0;

    virtual CodecInfo getCodecInfo() const = 0;

    virtual std::vector<double> getIFrameTimestamps() const = 0;

    virtual std::vector<double> getPFrameTimestamps() const = 0;
};