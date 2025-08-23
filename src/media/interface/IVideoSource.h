#pragma once

#include <string>
#include <memory>
#include "media/ThreadSafeQueue.h"
#include "media/VideoFrame.h"
#include "media/AudioFrame.h"
#include "media/CodecInfo.h"

class IVideoSource {
public:
    virtual ~IVideoSource() = default;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual void startRecord() = 0;

    virtual void stopRecord() = 0;

    virtual void flush() = 0;

    virtual bool seek(double timeInSeconds) = 0;

    virtual double getDuration() const = 0;

    virtual CodecInfo getCodecInfo() const = 0;

    virtual ThreadSafeQueue<VideoFrame> &getVideoQueue() = 0;

    virtual ThreadSafeQueue<AudioFrame> &getAudioQueue() = 0;
};