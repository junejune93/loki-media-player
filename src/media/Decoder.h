#pragma once

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>

#include "AudioFrame.h"
#include "ThreadSafeQueue.h"
#include "VideoFrame.h"
#include "interface/IDecoderSource.h"
#include "ui/OSDState.h"

struct VideoFrame;
struct AudioFrame;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

struct SeekRequest {
    std::atomic<bool> requested{false};
    std::atomic<double> target{0.0};

    void set(const double time) {
        target.store(time);
        requested.store(true);
    }

    std::pair<bool, double> get() const {
        if (bool req = requested.load()) {
            return {true, target.load()};
        }
        return {false, 0.0};
    }

    void clear() { requested.store(false); }
};

class Decoder final : public IDecoderSource {
public:
    explicit Decoder(std::string filename, DecoderConfig config = {});
    ~Decoder() override;

    void start() override;
    void stop() override;
    void flush() override;

    double getDuration() const override;
    bool seek(double timeInSeconds) override;

    ThreadSafeQueue<VideoFrame> &getVideoQueue() override { return _videoQueue; }
    ThreadSafeQueue<AudioFrame> &getAudioQueue() override { return _audioQueue; }

    CodecInfo getCodecInfo() const override;
    std::vector<double> getIFrameTimestamps() const override;
    std::vector<double> getPFrameTimestamps() const override;

private:
    struct DecodingState {
        bool isFirstAudioFrame = true;
        double audioStartPTS = 0.0;
        std::chrono::high_resolution_clock::time_point playbackStartTime;

        void reset() {
            isFirstAudioFrame = true;
            audioStartPTS = 0.0;
            playbackStartTime = std::chrono::high_resolution_clock::now();
        }
    };

    static void initializeFFmpeg();
    void openInputFile();
    void findStreams();
    void scanFrameTypes();
    void initializeVideoDecoder();
    void initializeVideoScaler(AVPixelFormat srcFmt);
    void initializeAudioDecoder();
    void initializeAudioResampler(const AVStream *audioStream);

    void initializeCodecInfo();
    void extractContainerFormat();
    void extractVideoInfo();
    void extractAudioInfo();
    static std::string normalizeVideoCodecName(const std::string &codecName);
    static std::string normalizeAudioCodecName(const std::string &codecName);

    void scanForFrameTypes(AVFormatContext *fmtCtx, int videoStreamIndex, std::vector<double> &timestamps);
    void clearFrameTimestamps();
    void addIFrameTimestamp(double pts);
    void addPFrameTimestamp(double pts);
    void sortFrameTimestamps();

    void startDecoding();
    bool handleSeekRequest(DecodingState &state);
    bool waitForQueueSpace() const;
    void decodeAudioPacket(const AVPacket *packet, AVFrame *frame, DecodingState &state);
    void decodeVideoPacket(const AVPacket *packet, AVFrame *frame, const DecodingState &state);
    std::optional<AudioFrame> createAudioFrame(const AVFrame *frame, DecodingState &state) const;
    std::optional<VideoFrame> createVideoFrame(const AVFrame *frame, const DecodingState &state) const;
    static void syncVideoFrame(const VideoFrame &videoFrame, const DecodingState &state);

    void cleanup();
    int getMaxQueueSize() const;

    // CUDA
    static enum AVPixelFormat getHWFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);
    bool initializeCudaDevice();
    bool initializeCudaFrames(AVCodecContext *ctx) const;
    static AVPixelFormat pickSWFormatForCuda(AVPixelFormat hw_mapped);
    void recreateVideoScalerIfNeeded(AVPixelFormat srcFmt, int w, int h);

private:
    std::string filename;
    DecoderConfig _config;

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

    SeekRequest _seekRequest;

    CodecInfo _codecInfo;
    std::vector<double> _iFrameTimestamps;
    std::vector<double> _pFrameTimestamps;
    mutable std::mutex _iFrameTimestampsMutex;
    mutable std::mutex _pFrameTimestampsMutex;

    // CUDA
    AVBufferRef *_hwDeviceCtx{nullptr};
    AVPixelFormat _currentScaleSrcFmt{AV_PIX_FMT_NONE};
    bool _useHW{false};

    mutable std::mutex _swsCtxMutex;
    mutable std::mutex _cudaMutex;
    mutable std::mutex _swrCtxMutex;

    static constexpr int8_t MAX_QUEUE_SIZE_HD = 50;
    static constexpr int8_t MAX_QUEUE_SIZE_4K = 20;
};