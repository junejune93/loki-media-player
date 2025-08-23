#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include "ThreadSafeQueue.h"
#include "VideoFrame.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class Encoder {
public:
    enum class OutputFormat {
        MP4,
        MKV,
        MOV
    };

    explicit Encoder(std::string  outputDir,
            int segmentDuration = 180,  // 3 minutes
            OutputFormat format = OutputFormat::MP4);
    
    ~Encoder();

    bool initialize(int width, int height, int fps, AVPixelFormat inputFormat);
    
    void encodeFrame(const VideoFrame& frame);
    
    void finalize();
    
    bool isInitialized() const { return _initialized; }
    
    std::string getCurrentOutputPath() const;

private:
    struct OutputStream {
        AVStream* stream{nullptr};
        AVCodecContext* enc{nullptr};
        int64_t nextPts{0};
    };

    bool setupCodec(int width, int height, int fps, AVPixelFormat inputFormat);
    bool openOutputFile();
    void closeOutputFile();
    void startNewSegment();
    
    bool convertFrame(const VideoFrame& frame, AVFrame* dstFrame);
    
    std::string generateOutputFilename() const;
    
    std::string _outputDir;
    int _segmentDuration;
    int64_t _segmentStartPts{0};
    int64_t _nextSegmentPts{0};
    OutputFormat _outputFormat;
    
    AVFormatContext* _fmtCtx{nullptr};
    OutputStream _videoStream;
    SwsContext* _swsCtx{nullptr};
    AVFrame* _frame{nullptr};
    AVPacket* _pkt{nullptr};
    
    std::atomic<bool> _initialized{false};
    std::mutex _mutex;
    int _frameCounter{0};
    int _currentSegment{0};
    
    int _width{0};
    int _height{0};
    int _fps{0};
    AVPixelFormat _inputFormat{AV_PIX_FMT_NONE};
    
    std::vector<uint8_t> _conversionBuffer;
};
