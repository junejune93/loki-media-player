#pragma once

#include <string>
#include <thread>
#include <atomic>
#include "ThreadSafeQueue.h"
#include "VideoFrame.h"
#include "AudioFrame.h"

struct VideoFrame;
struct AudioFrame;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

class Decoder {
public:
    explicit Decoder(std::string filename);

    ~Decoder();

    void start();

    void stop();

    void flush();

    double getDuration() const;

    bool seek(double timeInSeconds);

    ThreadSafeQueue<VideoFrame> &getVideoQueue() { return _videoQueue; }

    ThreadSafeQueue<AudioFrame> &getAudioQueue() { return _audioQueue; }

private:
    void startDecoding();

    std::string filename;

    AVFormatContext *_fmtCtx{nullptr};
    AVCodecContext *_videoCtx{nullptr};
    AVCodecContext *_audioCtx{nullptr};
    SwrContext *_swrCtx{nullptr};
    SwsContext *_swsCtx{nullptr};

    int _videoStreamIndex{-1};
    int _audioStreamIndex{-1};
    AVRational _videoTimeBase{};
    AVRational _audioTimeBase{};

    ThreadSafeQueue<VideoFrame> _videoQueue;
    ThreadSafeQueue<AudioFrame> _audioQueue;

    std::thread _decodeThread;
    std::atomic<bool> _decodeRunning{false};
    std::atomic<bool> _seekRequested{false};
    std::atomic<double> _seekTarget{0.0};
};