#include "Encoder.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace fs = std::filesystem;

using namespace std::chrono;

Encoder::Encoder(std::string  outputDir, int segmentDuration, OutputFormat format)
    : _outputDir(std::move(outputDir)),
    _segmentDuration(segmentDuration),
    _outputFormat(format) {
    if (!_outputDir.empty() && _outputDir.back() != '/' && _outputDir.back() != '\\') {
        _outputDir += fs::path::preferred_separator;
    }
    
    fs::create_directories(_outputDir);
    
    _frame = av_frame_alloc();
    _pkt = av_packet_alloc();
    if (!_frame || !_pkt) {
        throw std::runtime_error("Could not allocate frame or packet");
    }
    
    _conversionBuffer.reserve(1920 * 1080 * 4 /* RGB+A */);
}

Encoder::~Encoder() {
    finalize();
    if (_frame) {
        av_frame_free(&_frame);
    }

    if (_pkt) {
        av_packet_free(&_pkt);
    }

    if (_swsCtx) {
        sws_freeContext(_swsCtx);
    }
}

bool Encoder::initialize(int width, int height, int fps, AVPixelFormat inputFormat) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (width <= 0 || height <= 0 || fps <= 0) {
        std::cerr << "Invalid video parameters" << std::endl;
        return false;
    }
    
    _width = width;
    _height = height;
    _fps = fps;
    _inputFormat = inputFormat;
    
    if (!setupCodec(width, height, fps, false, inputFormat)) {
        std::cerr << "Failed to set up codec" << std::endl;
        return false;
    }
    
    _initialized = true;
    return true;
}

bool Encoder::setupCodec(int width, int height, int fps, bool bframe, AVPixelFormat pixFmt) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        return false;
    }
    
    _videoStream.enc = avcodec_alloc_context3(codec);
    if (!_videoStream.enc) {
        return false;
    }
    
    _videoStream.enc->codec_id = AV_CODEC_ID_H264;
    _videoStream.enc->bit_rate = 4000000;
    _videoStream.enc->width = width;
    _videoStream.enc->height = height;
    _videoStream.enc->time_base = (AVRational){1, fps};
    _videoStream.enc->framerate = (AVRational){fps, 1};
    _videoStream.enc->gop_size = fps * 2;
    _videoStream.enc->max_b_frames = bframe ? 2 : 0;
    _videoStream.enc->pix_fmt = AV_PIX_FMT_YUV420P;
    
    av_opt_set(_videoStream.enc->priv_data, "preset", "slow", 0);
    av_opt_set(_videoStream.enc->priv_data, "crf", "23", 0);
    av_opt_set(_videoStream.enc->priv_data, "profile", "high", 0);
    av_opt_set(_videoStream.enc->priv_data, "level", "4.0", 0);

    if (bframe) {
        av_opt_set(_videoStream.enc->priv_data, "bframes", "2", 0);
        av_opt_set(_videoStream.enc->priv_data, "b-adapt", "1", 0);
        av_opt_set(_videoStream.enc->priv_data, "b-pyramid", "normal", 0);
    } else {
        av_opt_set(_videoStream.enc->priv_data, "tune", "zerolatency", 0);
        av_opt_set(_videoStream.enc->priv_data, "bframes", "0", 0);
        av_opt_set(_videoStream.enc->priv_data, "b-adapt", "0", 0);
        av_opt_set(_videoStream.enc->priv_data, "b-pyramid", "none", 0);
    }

    if (avcodec_open2(_videoStream.enc, codec, nullptr) < 0) {
        std::cerr << "Could not open codec" << std::endl;
        return false;
    }
    
    if (pixFmt != AV_PIX_FMT_YUV420P) {
        _swsCtx = sws_getContext(width, height, pixFmt, width, height, AV_PIX_FMT_YUV420P,
                                SWS_BICUBIC, nullptr, nullptr, nullptr);
        if (!_swsCtx) {
            std::cerr << "Could not initialize the conversion context" << std::endl;
            return false;
        }
    }
    
    return true;
}

std::string Encoder::generateOutputFilename() const {
    auto now = system_clock::now();
    auto now_time = system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&now_time);
    
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_part" << _currentSegment;
    
    switch (_outputFormat) {
        case OutputFormat::MP4: ss << ".mp4"; break;
        case OutputFormat::MKV: ss << ".mkv"; break;
        case OutputFormat::MOV: ss << ".mov"; break;
    }
    
    return _outputDir + ss.str();
}

bool Encoder::convertFrame(const VideoFrame& frame, AVFrame* dstFrame) {
    if (!dstFrame || frame.data.empty()) {
        return false;
    }

    if (dstFrame->data[0] == nullptr) {
        dstFrame->format = AV_PIX_FMT_YUV420P;
        dstFrame->width = frame.width;
        dstFrame->height = frame.height;

        if (av_frame_get_buffer(dstFrame, 32) < 0) {
            std::cerr << "Could not allocate frame data" << std::endl;
            return false;
        }
    }

    const auto rgb_size = frame.width * frame.height * 3;
    const auto rgba_size = frame.width * frame.height * 4;
    const auto yuv420p_size = frame.width * frame.height * 3 / 2;

    if (frame.data.size() == rgba_size) {
        if (!_swsCtx) {
            _swsCtx = sws_getContext(frame.width, frame.height, AV_PIX_FMT_RGBA, dstFrame->width, dstFrame->height,
                                     AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);

            if (!_swsCtx) {
                return false;
            }
        }

        const uint8_t *srcData[1] = {frame.data.data()};
        int srcLinesize[1] = {frame.width * 4}; // RGBA

        sws_scale(_swsCtx, srcData, srcLinesize, 0, frame.height, dstFrame->data, dstFrame->linesize);

        return true;
    } else if (frame.data.size() == rgb_size) {
        if (_swsCtx) {
            sws_freeContext(_swsCtx);
            _swsCtx = nullptr;
        }

        int flags = SWS_BICUBIC | SWS_ACCURATE_RND | SWS_FULL_CHR_H_INT;
        _swsCtx = sws_getContext(frame.width, frame.height, AV_PIX_FMT_RGB24, frame.width, frame.height,
                                 AV_PIX_FMT_YUV420P, flags, nullptr, nullptr, nullptr);

        if (!_swsCtx) {
            return false;
        }

        av_frame_unref(dstFrame);
        dstFrame->format = AV_PIX_FMT_YUV420P;
        dstFrame->width = frame.width;
        dstFrame->height = frame.height;

        if (av_frame_get_buffer(dstFrame, 32) < 0) {
            return false;
        }

        const uint8_t *srcData[1] = {frame.data.data()};
        int srcLinesize[1] = {static_cast<int>(frame.width * 3)};

        int result = sws_scale(_swsCtx, srcData, srcLinesize, 0, frame.height, dstFrame->data, dstFrame->linesize);

        if (result <= 0) {
            return false;
        }

        dstFrame->pts =
                static_cast<int64_t>(frame.pts * _videoStream.enc->time_base.den / _videoStream.enc->time_base.num);

        return true;
    } else if (frame.data.size() == yuv420p_size) {
        size_t y_size = frame.width * frame.height;
        size_t uv_size = y_size / 4;

        // Y
        memcpy(dstFrame->data[0], frame.data.data(), y_size);
        // UV
        memcpy(dstFrame->data[1], frame.data.data() + y_size, uv_size);
        memcpy(dstFrame->data[2], frame.data.data() + y_size + uv_size, uv_size);

        return true;
    }

    return false;
}

bool Encoder::openOutputFile() {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&now_time);

    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_part" << _currentSegment;

    // Record 파일 확장자 결정
    switch (_outputFormat) {
        case OutputFormat::MP4: ss << ".mp4"; break;
        case OutputFormat::MKV: ss << ".mkv"; break;
        case OutputFormat::MOV: ss << ".mov"; break;
        default: ss << ".mp4"; break;
    }

    std::string outputPath = _outputDir + ss.str();

    // 포맷 문자열 설정
    const char* formatName = nullptr;
    switch (_outputFormat) {
        case OutputFormat::MP4: formatName = "mp4"; break;
        case OutputFormat::MKV: formatName = "matroska"; break;
        case OutputFormat::MOV: formatName = "mov"; break;
        default: formatName = "mp4"; break;
    }

    if (avformat_alloc_output_context2(&_fmtCtx, nullptr, formatName, outputPath.c_str()) < 0) {
        return false;
    }

    if (!_fmtCtx) {
        return false;
    }
    
    AVStream* stream = avformat_new_stream(_fmtCtx, nullptr);
    if (!stream) {
        avformat_free_context(_fmtCtx);
        _fmtCtx = nullptr;
        return false;
    }
    
    if (avcodec_parameters_from_context(stream->codecpar, _videoStream.enc) < 0) {
        avformat_free_context(_fmtCtx);
        _fmtCtx = nullptr;
        return false;
    }

    // 파일 열기
    if (!(_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&_fmtCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            avformat_free_context(_fmtCtx);
            _fmtCtx = nullptr;
            return false;
        }
    }

    // 헤더 쓰기
    if (avformat_write_header(_fmtCtx, nullptr) < 0) {
        std::cerr << "Error writing header" << std::endl;
        if (!(_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&_fmtCtx->pb);
        }
        avformat_free_context(_fmtCtx);
        _fmtCtx = nullptr;
        return false;
    }
    
    _videoStream.stream = stream;

    std::cout << "Started new segment: " << outputPath << std::endl;
    return true;
}

void Encoder::closeOutputFile() {
    if (!_fmtCtx) {
        return;
    }
    
    av_write_trailer(_fmtCtx);
    
    if (!(_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&_fmtCtx->pb);
    }
    
    avformat_free_context(_fmtCtx);
    _fmtCtx = nullptr;
    _videoStream.stream = nullptr;
    
    _currentSegment++;
}

void Encoder::startNewSegment() {
    if (_fmtCtx) {
        closeOutputFile();
    }

    const auto success = openOutputFile();
    if (!success) {
        return;
    }

    _segmentStartPts = _videoStream.nextPts;
}

void Encoder::encodeFrame(const VideoFrame& frame) {
    if (!_initialized) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(_mutex);

    // 최초 프레임 또는 세그먼트 시간 초과
    bool needNewSegment = !_fmtCtx || (_videoStream.nextPts - _segmentStartPts) >= _segmentDuration * _fps;
    if (needNewSegment) {
        std::cout << "Need new segment. Current PTS: " << _videoStream.nextPts
                  << ", Segment start PTS: " << _segmentStartPts
                  << ", Duration threshold: " << (_segmentDuration * _fps) << std::endl;
        startNewSegment();

        if (!_fmtCtx) {
            return;
        }

        if (!_videoStream.stream) {
            return;
        }
    }

    if (!_videoStream.enc) {
        return;
    }

    if (!_frame->data[0]) {
        _frame->format = AV_PIX_FMT_YUV420P;
        _frame->width = _width;
        _frame->height = _height;
        if (av_frame_get_buffer(_frame, 32) < 0) {
            std::cerr << "Could not allocate frame data" << std::endl;
            return;
        }
    }
    
    if (!convertFrame(frame, _frame)) {
        return;
    }
    
    _frame->pts = _videoStream.nextPts++;
    _frame->pkt_dts = _frame->pts;
    
    int ret = avcodec_send_frame(_videoStream.enc, _frame);
    if (ret < 0) {
        return;
    }
    
    while (true) {
        ret = avcodec_receive_packet(_videoStream.enc, _pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            std::cerr << "Error during encoding" << std::endl;
            break;
        }

        if (!_videoStream.stream) {
            av_packet_unref(_pkt);
            break;
        }

        // PTS 조정
        av_packet_rescale_ts(_pkt, _videoStream.enc->time_base, _videoStream.stream->time_base);
        _pkt->stream_index = _videoStream.stream->index;

        // 패킷 쓰기
        ret = av_interleaved_write_frame(_fmtCtx, _pkt);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Error writing packet: " << errbuf << std::endl;
        }
        
        av_packet_unref(_pkt);
    }

    _frameCounter++;
}

void Encoder::finalize() {
    if (!_initialized) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(_mutex);
    
    try {
        if (_videoStream.enc) {
            avcodec_send_frame(_videoStream.enc, nullptr);
            
            // Get remaining packets
            while (true) {
                int ret = avcodec_receive_packet(_videoStream.enc, _pkt);
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Error flushing encoder" << std::endl;
                    break;
                }
                
                // Write remaining packets
                if (_fmtCtx && _videoStream.stream) {
                    av_packet_rescale_ts(_pkt, _videoStream.enc->time_base, _videoStream.stream->time_base);
                    _pkt->stream_index = _videoStream.stream->index;
                    
                    if (av_interleaved_write_frame(_fmtCtx, _pkt) < 0) {
                        std::cerr << "Error writing packet during finalize" << std::endl;
                    }
                }
                
                av_packet_unref(_pkt);
            }
        }
        
        closeOutputFile();
        
        if (_videoStream.enc) {
            avcodec_free_context(&_videoStream.enc);
            _videoStream.enc = nullptr;
        }
        
        if (_swsCtx) {
            sws_freeContext(_swsCtx);
            _swsCtx = nullptr;
        }

        _initialized = false;
        std::cout << "Encoder finalized. Total frames encoded: " << _frameCounter << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error in Encoder::finalize: " << e.what() << std::endl;
    }
}
