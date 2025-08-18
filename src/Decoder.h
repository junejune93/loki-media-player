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
    explicit Decoder(const std::string& filename);
    ~Decoder();

    void start();
    void stop();

    ThreadSafeQueue<VideoFrame>& getVideoQueue() { return videoQueue; }
    ThreadSafeQueue<AudioFrame>& getAudioQueue() { return audioQueue; }

private:
    void startDecoding();

    std::string filename;

    AVFormatContext* fmtCtx{nullptr};
    AVCodecContext* videoCtx{nullptr};
    AVCodecContext* audioCtx{nullptr};
    SwrContext*     swrCtx{nullptr};
    SwsContext*     swsCtx{nullptr};

    int videoStreamIndex{-1};
    int audioStreamIndex{-1};
    AVRational videoTimeBase{};
    AVRational audioTimeBase{};

    ThreadSafeQueue<VideoFrame> videoQueue;
    ThreadSafeQueue<AudioFrame> audioQueue;

    std::thread decodeThread;
    std::atomic<bool> running{false};
};