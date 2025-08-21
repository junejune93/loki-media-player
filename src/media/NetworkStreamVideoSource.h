#pragma once

#include "media/interface/IVideoSource.h"
#include "Decoder.h"
#include <memory>

class NetworkStreamVideoSource : public IVideoSource {
public:
    explicit NetworkStreamVideoSource(const std::string &uri);

    ~NetworkStreamVideoSource() override;

    void start() override;

    void stop() override;

    void flush() override;

    bool seek(double t) override;

    double getDuration() const override;

    CodecInfo getCodecInfo() const override;

    ThreadSafeQueue<VideoFrame> &getVideoQueue() override;

    ThreadSafeQueue<AudioFrame> &getAudioQueue() override;

private:
    std::unique_ptr<Decoder> _decoder;
};