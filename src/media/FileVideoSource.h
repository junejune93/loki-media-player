#pragma once

#include "media/interface/IVideoSource.h"
#include "Decoder.h"
#include <memory>

class FileVideoSource : public IVideoSource {
public:
    explicit FileVideoSource(const std::string &filename);

    ~FileVideoSource() override;

    void start() override;

    void stop() override;

    void flush() override;

    bool seek(double timeInSeconds) override;

    double getDuration() const override;

    CodecInfo getCodecInfo() const override;

    ThreadSafeQueue<VideoFrame> &getVideoQueue() override;

    ThreadSafeQueue<AudioFrame> &getAudioQueue() override;

    Decoder &decoder();

private:
    std::unique_ptr<Decoder> _decoder;
};