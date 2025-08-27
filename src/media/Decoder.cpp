#include "Decoder.h"
#include "VideoRenderer.h"
#include "AudioPlayer.h"
#include <stdexcept>
#include <utility>
#include <spdlog/spdlog.h>

Decoder::Decoder(std::string file, const DecoderConfig& config) 
    : filename(std::move(file)), _config(config) {
    avformat_network_init();

    if (avformat_open_input(&_fmtCtx, filename.c_str(), nullptr, nullptr) != 0)
        throw std::runtime_error("Failed to open input file");

    if (avformat_find_stream_info(_fmtCtx, nullptr) < 0)
        throw std::runtime_error("Failed to get _stream info");

    // Find streams
    for (unsigned i = 0; i < _fmtCtx->nb_streams; ++i) {
        AVCodecParameters *par = _fmtCtx->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && _videoStreamIndex < 0) {
            _videoStreamIndex = static_cast<int>(i);
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && _audioStreamIndex < 0) {
            _audioStreamIndex = static_cast<int>(i);
        }
    }
    if (_videoStreamIndex < 0)
        throw std::runtime_error("No video _stream found");

    // Scan Frame Types
    {
        if (_videoStreamIndex >= 0) {
            scanForFrameTypes(_fmtCtx, _videoStreamIndex, _iFrameTimestamps);
        }
    }

    // Video codec
    {
        AVStream *vs = _fmtCtx->streams[_videoStreamIndex];
        const AVCodec *vcodec = avcodec_find_decoder(vs->codecpar->codec_id);
        if (!vcodec) throw std::runtime_error("Video codec not found");
        _videoCtx = avcodec_alloc_context3(vcodec);
        avcodec_parameters_to_context(_videoCtx, vs->codecpar);
        if (avcodec_open2(_videoCtx, vcodec, nullptr) < 0)
            throw std::runtime_error("Failed to open video codec");
        _videoTimeBase = vs->time_base;

        _swsCtx = sws_getContext(
                _videoCtx->width, _videoCtx->height, _videoCtx->pix_fmt,
                _videoCtx->width, _videoCtx->height, AV_PIX_FMT_RGB24,
                SWS_BILINEAR, nullptr, nullptr, nullptr
        );
    }

    // Audio codec (optional)
    if (_audioStreamIndex >= 0) {
        AVStream *as = _fmtCtx->streams[_audioStreamIndex];
        const AVCodec *acodec = avcodec_find_decoder(as->codecpar->codec_id);
        if (!acodec) throw std::runtime_error("Audio codec not found");
        _audioCtx = avcodec_alloc_context3(acodec);
        avcodec_parameters_to_context(_audioCtx, as->codecpar);
        if (avcodec_open2(_audioCtx, acodec, nullptr) < 0)
            throw std::runtime_error("Failed to open audio codec");
        _audioTimeBase = as->time_base;

        // Swr: to S16 stereo @ input sample_rate
        uint64_t in_ch_layout = as->codecpar->channel_layout;
        if (in_ch_layout == 0)
            in_ch_layout = av_get_default_channel_layout(as->codecpar->channels);

        _swrCtx = swr_alloc();
        if (!_swrCtx) throw std::runtime_error("Failed to alloc _swrCtx");

        av_opt_set_int(_swrCtx, "in_channel_layout", (long) in_ch_layout, 0);
        av_opt_set_int(_swrCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        av_opt_set_int(_swrCtx, "in_sample_rate", _audioCtx->sample_rate, 0);
        av_opt_set_int(_swrCtx, "out_sample_rate", _audioCtx->sample_rate, 0);
        av_opt_set_sample_fmt(_swrCtx, "in_sample_fmt", _audioCtx->sample_fmt, 0);
        av_opt_set_sample_fmt(_swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

        if (swr_init(_swrCtx) < 0)
            throw std::runtime_error("Failed to init swr");
    }

    initializeCodecInfo();
}

Decoder::~Decoder() {
    stop();
    if (_swsCtx) sws_freeContext(_swsCtx);
    if (_videoCtx) avcodec_free_context(&_videoCtx);
    if (_audioCtx) avcodec_free_context(&_audioCtx);
    if (_swrCtx) swr_free(&_swrCtx);
    if (_fmtCtx) avformat_close_input(&_fmtCtx);
    avformat_network_deinit();
}

void Decoder::initializeCodecInfo() {
    _codecInfo.clear();

    if (!_fmtCtx) return;

    if (_fmtCtx->iformat && _fmtCtx->iformat->name) {
        _codecInfo.containerFormat = std::string(_fmtCtx->iformat->name);
        if (_codecInfo.containerFormat == "mov,mp4,m4a,3gp,3g2,mj2") {
            _codecInfo.containerFormat = "MP4";
        } else if (_codecInfo.containerFormat == "matroska,webm") {
            _codecInfo.containerFormat = "MKV/WebM";
        } else if (_codecInfo.containerFormat == "avi") {
            _codecInfo.containerFormat = "AVI";
        }
    }

    // video
    if (_videoStreamIndex >= 0 && _videoCtx) {
        _codecInfo.hasVideo = true;
        AVStream *vs = _fmtCtx->streams[_videoStreamIndex];

        if (_videoCtx->codec && _videoCtx->codec->name) {
            _codecInfo.videoCodec = std::string(_videoCtx->codec->name);
            if (_codecInfo.videoCodec == "h264") {
                _codecInfo.videoCodec = "H.264/AVC";
            } else if (_codecInfo.videoCodec == "hevc") {
                _codecInfo.videoCodec = "H.265/HEVC";
            } else if (_codecInfo.videoCodec == "vp9") {
                _codecInfo.videoCodec = "VP9";
            } else if (_codecInfo.videoCodec == "vp8") {
                _codecInfo.videoCodec = "VP8";
            } else if (_codecInfo.videoCodec == "av1") {
                _codecInfo.videoCodec = "AV1";
            } else {
                _codecInfo.videoCodec = "UNKNOWN";
            }
        }

        // resolution
        if (_videoCtx->width > 0 && _videoCtx->height > 0) {
            _codecInfo.videoResolution = std::to_string(_videoCtx->width) + "x" + std::to_string(_videoCtx->height);
        }

        // bitrate
        if (vs->codecpar->bit_rate > 0) {
            _codecInfo.videoBitrate = CodecInfo::formatBitrate(vs->codecpar->bit_rate);
        } else if (_fmtCtx->bit_rate > 0) {
            _codecInfo.videoBitrate = CodecInfo::formatBitrate((int64_t )((double)(_fmtCtx->bit_rate) * 0.8));
        }
    }

    // audio
    if (_audioStreamIndex >= 0 && _audioCtx) {
        _codecInfo.hasAudio = true;
        AVStream *as = _fmtCtx->streams[_audioStreamIndex];

        if (_audioCtx->codec && _audioCtx->codec->name) {
            _codecInfo.audioCodec = std::string(_audioCtx->codec->name);
            if (_codecInfo.audioCodec == "aac") {
                _codecInfo.audioCodec = "AAC";
            } else if (_codecInfo.audioCodec == "mp3") {
                _codecInfo.audioCodec = "MP3";
            } else if (_codecInfo.audioCodec == "ac3") {
                _codecInfo.audioCodec = "AC-3";
            } else if (_codecInfo.audioCodec == "eac3") {
                _codecInfo.audioCodec = "E-AC-3";
            } else if (_codecInfo.audioCodec == "dts") {
                _codecInfo.audioCodec = "DTS";
            } else if (_codecInfo.audioCodec == "opus") {
                _codecInfo.audioCodec = "Opus";
            } else if (_codecInfo.audioCodec == "vorbis") {
                _codecInfo.audioCodec = "Vorbis";
            } else {
                _codecInfo.audioCodec = "UNKNOWN";
            }
        }

        // samplerate
        if (_audioCtx->sample_rate > 0) {
            _codecInfo.audioSampleRate = CodecInfo::formatSampleRate(_audioCtx->sample_rate);
        }

        // bitrate
        if (as->codecpar->bit_rate > 0) {
            _codecInfo.audioBitrate = CodecInfo::formatBitrate(as->codecpar->bit_rate);
        }

        if (_audioCtx->channels > 0) {
            uint64_t ch_layout = as->codecpar->channel_layout;
            if (ch_layout == 0) {
                ch_layout = av_get_default_channel_layout(_audioCtx->channels);
            }
            _codecInfo.audioChannels = CodecInfo::formatChannelLayout(_audioCtx->channels, ch_layout);
        }
    }
}

void Decoder::scanForFrameTypes(AVFormatContext *fmtCtx, int videoStreamIndex, std::vector<double> &timestamps) {
    av_seek_frame(fmtCtx, videoStreamIndex, 0, AVSEEK_FLAG_FRAME);
    AVPacket *packet = av_packet_alloc();

    // clear
    {
        std::lock_guard<std::mutex> lock1(_iFrameTimestampsMutex);
        _iFrameTimestamps.clear();
    }

    {
        std::lock_guard<std::mutex> lock2(_pFrameTimestampsMutex);
        _pFrameTimestamps.clear();
    }

    // read
    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            double pts = (double)(packet->pts) * av_q2d(fmtCtx->streams[videoStreamIndex]->time_base);
            if (packet->flags & AV_PKT_FLAG_KEY) {
                // I-Frame
                std::lock_guard<std::mutex> lock(_iFrameTimestampsMutex);
                _iFrameTimestamps.push_back(pts);
            } else if (packet->flags & AV_PKT_FLAG_DISCARD) {
                ;
            } else {
                // P/B-Frame
                std::lock_guard<std::mutex> lock(_pFrameTimestampsMutex);
                _pFrameTimestamps.push_back(pts);
            }
        }
        av_packet_unref(packet);
    }

    // Sort - Frame(I)
    {
        std::lock_guard<std::mutex> lock1(_iFrameTimestampsMutex);
        std::sort(_iFrameTimestamps.begin(), _iFrameTimestamps.end());
    }

    // Sort - Frame(P,B)
    {
        std::lock_guard<std::mutex> lock2(_pFrameTimestampsMutex);
        std::sort(_pFrameTimestamps.begin(), _pFrameTimestamps.end());
    }

    {
        timestamps = _iFrameTimestamps;
    }

    av_packet_free(&packet);
    av_seek_frame(fmtCtx, videoStreamIndex, 0, AVSEEK_FLAG_FRAME);
}

CodecInfo Decoder::getCodecInfo() const {
    return _codecInfo;
}

void Decoder::start() {
    if (_decodeRunning) {
        return;
    }
    _decodeRunning = true;
    _decodeThread = std::thread(&Decoder::startDecoding, this);
}

void Decoder::stop() {
    if (!_decodeRunning) {
        return;
    }

    _decodeRunning = false;

    if (_decodeThread.joinable()) {
        _decodeThread.join();
    }
}

double Decoder::getDuration() const {
    if (!_fmtCtx) {
        return 0.0;
    }

    if (_fmtCtx->duration != AV_NOPTS_VALUE) {
        return static_cast<double>(_fmtCtx->duration) / AV_TIME_BASE;
    }

    if (_videoStreamIndex >= 0) {
        AVStream *vs = _fmtCtx->streams[_videoStreamIndex];
        if (vs->duration != AV_NOPTS_VALUE) {
            return static_cast<double>(vs->duration) * av_q2d(vs->time_base);
        }
    }

    return 0.0;
}

bool Decoder::seek(double timeInSeconds) {
    if (!_fmtCtx || !_decodeRunning) {
        return false;
    }

    _seekTarget = timeInSeconds;
    _seekRequested = true;

    return true;
}

std::vector<double> Decoder::getIFrameTimestamps() const {
    std::lock_guard<std::mutex> lock(_iFrameTimestampsMutex);
    return _iFrameTimestamps;
}

std::vector<double> Decoder::getPFrameTimestamps() const {
    std::lock_guard<std::mutex> lock(_pFrameTimestampsMutex);
    return _pFrameTimestamps;
}

void Decoder::startDecoding() {
    AVPacket *packet = av_packet_alloc();
    AVFrame *vfr = av_frame_alloc();
    AVFrame *afr = av_frame_alloc();

    const size_t MAX_QUEUE_SIZE = 50;
    bool firstAudioFrame = true;
    double audioStartPTS = 0.0;

    auto playbackStartTime = std::chrono::high_resolution_clock::now();

    while (_decodeRunning && av_read_frame(_fmtCtx, packet) >= 0) {
        // SEEK
        if (_seekRequested.load()) {
            double seekTime = _seekTarget.load();
            auto seekTimestamp = static_cast<int64_t>(seekTime * AV_TIME_BASE);
            if (av_seek_frame(_fmtCtx, -1, seekTimestamp, AVSEEK_FLAG_BACKWARD) >= 0) {
                if (_videoCtx) {
                    avcodec_flush_buffers(_videoCtx);
                }
                if (_audioCtx) {
                    avcodec_flush_buffers(_audioCtx);
                }
                firstAudioFrame = true;
                audioStartPTS = 0.0;
                playbackStartTime = std::chrono::high_resolution_clock::now();
            }

            _seekRequested = false;
            av_packet_unref(packet);
            continue;
        }

        while (_decodeRunning && (_videoQueue.size() > MAX_QUEUE_SIZE || _audioQueue.size() > MAX_QUEUE_SIZE))
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (!_decodeRunning) break;

        // ---------------- AUDIO ----------------
        if (_audioCtx && packet->stream_index == _audioStreamIndex) {
            if (avcodec_send_packet(_audioCtx, packet) >= 0) {
                while (avcodec_receive_frame(_audioCtx, afr) >= 0) {
                    AudioFrame af;
                    af.sampleRate = _audioCtx->sample_rate;
                    af.channels = 2;
                    af.samples = afr->nb_samples;
                    af.pts = (afr->best_effort_timestamp == AV_NOPTS_VALUE) ? 0.0
                                                                            : (double) afr->best_effort_timestamp *
                                                                              av_q2d(_audioTimeBase);

                    if (firstAudioFrame) {
                        audioStartPTS = af.pts;
                        firstAudioFrame = false;
                        playbackStartTime = std::chrono::high_resolution_clock::now();
                    }

                    int outSamples = swr_get_out_samples(_swrCtx, afr->nb_samples);
                    int outBytes = av_samples_get_buffer_size(nullptr, af.channels, outSamples, AV_SAMPLE_FMT_S16, 1);
                    af.data.resize(outBytes);
                    uint8_t *outData[1] = {af.data.data()};
                    int convertedSamples = swr_convert(_swrCtx, outData, outSamples, (const uint8_t **) afr->data,
                                                       afr->nb_samples);

                    if (convertedSamples > 0) {
                        af.data.resize(convertedSamples * af.channels * sizeof(int16_t));
                        af.samples = convertedSamples;
                        _audioQueue.push(std::move(af));
                    }
                }
            }
        }

            // ---------------- VIDEO ----------------
        else if (packet->stream_index == _videoStreamIndex) {
            if (avcodec_send_packet(_videoCtx, packet) >= 0) {
                while (avcodec_receive_frame(_videoCtx, vfr) >= 0) {
                    VideoFrame vf;
                    vf.width = vfr->width;
                    vf.height = vfr->height;
                    vf.pts = (vfr->best_effort_timestamp == AV_NOPTS_VALUE) ? 0.0
                                                                            : (double) vfr->best_effort_timestamp *
                                                                              av_q2d(_videoTimeBase);

                    vf.data.resize(vf.width * vf.height * 3);
                    uint8_t *dest[1] = {vf.data.data()};
                    int lines[1] = {3 * vf.width};
                    sws_scale(_swsCtx, vfr->data, vfr->linesize, 0, vf.height, dest, lines);

                    if (!firstAudioFrame) {
                        double relativeVideoPTS = vf.pts - audioStartPTS;
                        auto now = std::chrono::high_resolution_clock::now();
                        double elapsed = std::chrono::duration<double>(now - playbackStartTime).count();

                        if (relativeVideoPTS > elapsed) {
                            std::this_thread::sleep_for(std::chrono::duration<double>(relativeVideoPTS - elapsed));
                        }
                    }

                    _videoQueue.push(std::move(vf));
                }
            }
        }

        av_packet_unref(packet);
    }

    av_frame_free(&vfr);
    av_frame_free(&afr);
    av_packet_free(&packet);
}

void Decoder::flush() {
    if (_videoCtx) {
        avcodec_flush_buffers(_videoCtx);
    }

    if (_audioCtx) {
        avcodec_flush_buffers(_audioCtx);
    }
}