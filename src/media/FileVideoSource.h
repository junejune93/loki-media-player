#pragma once

#include <memory>
#include <string>
#include "Decoder.h"
#include "Encoder.h"
#include "media/interface/IVideoSource.h"

class FileVideoSource : public IVideoSource {
public:
    explicit FileVideoSource(const std::string &filename);

    ~FileVideoSource() override;

    void start() override;

    void stop() override;

    void startRecord() override;

    void stopRecord() override;

    void flush() override;

    bool seek(double timeInSeconds) override;

    double getDuration() const override;

    CodecInfo getCodecInfo() const override;

    ThreadSafeQueue<VideoFrame> &getVideoQueue() override;

    ThreadSafeQueue<AudioFrame> &getAudioQueue() override;

    Decoder &decoder();

    void encodeFrame(const VideoFrame &frame) override;

    std::vector<double> getIFrameTimestamps() const override;

private:
    std::unique_ptr<Decoder> _decoder;
    std::unique_ptr<Encoder> _encoder;
    std::string _outputDir;
    bool _isRecording{false};
};
