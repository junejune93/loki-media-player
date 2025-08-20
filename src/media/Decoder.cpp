#include "Decoder.h"
#include "VideoRenderer.h"
#include "AudioPlayer.h"
#include <iostream>
#include <stdexcept>

Decoder::Decoder(const std::string& file) : filename(file) {
    avformat_network_init();

    if (avformat_open_input(&_fmtCtx, filename.c_str(), nullptr, nullptr) != 0)
        throw std::runtime_error("Failed to open input file");

    if (avformat_find_stream_info(_fmtCtx, nullptr) < 0)
        throw std::runtime_error("Failed to get _stream info");

    // Find streams
    for (unsigned i = 0; i < _fmtCtx->nb_streams; ++i) {
        AVCodecParameters* par = _fmtCtx->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && _videoStreamIndex < 0) {
            _videoStreamIndex = static_cast<int>(i);
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && _audioStreamIndex < 0) {
            _audioStreamIndex = static_cast<int>(i);
        }
    }
    if (_videoStreamIndex < 0)
        throw std::runtime_error("No video _stream found");

    // Video codec
    {
        AVStream* vs = _fmtCtx->streams[_videoStreamIndex];
        const AVCodec* vcodec = avcodec_find_decoder(vs->codecpar->codec_id);
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
        AVStream* as = _fmtCtx->streams[_audioStreamIndex];
        const AVCodec* acodec = avcodec_find_decoder(as->codecpar->codec_id);
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

        av_opt_set_int(_swrCtx, "in_channel_layout", in_ch_layout, 0);
        av_opt_set_int(_swrCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        av_opt_set_int(_swrCtx, "in_sample_rate", _audioCtx->sample_rate, 0);
        av_opt_set_int(_swrCtx, "out_sample_rate", _audioCtx->sample_rate, 0);
        av_opt_set_sample_fmt(_swrCtx, "in_sample_fmt", _audioCtx->sample_fmt, 0);
        av_opt_set_sample_fmt(_swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

        if (swr_init(_swrCtx) < 0)
            throw std::runtime_error("Failed to init swr");
    }
}

Decoder::~Decoder() {
    stop();
    if (_swsCtx)   sws_freeContext(_swsCtx);
    if (_videoCtx) avcodec_free_context(&_videoCtx);
    if (_audioCtx) avcodec_free_context(&_audioCtx);
    if (_swrCtx)   swr_free(&_swrCtx);
    if (_fmtCtx)   avformat_close_input(&_fmtCtx);
    avformat_network_deinit();
}

void Decoder::start() {
    if (_decodeRunning) return;
    _decodeRunning = true;
    _decodeThread = std::thread(&Decoder::startDecoding, this);
}

void Decoder::stop() {
    if (!_decodeRunning) return;
    _decodeRunning = false;
    if (_decodeThread.joinable()) _decodeThread.join();
}

double Decoder::getDuration() const {
    if (!_fmtCtx) {
        return 0.0;
    }

    if (_fmtCtx->duration != AV_NOPTS_VALUE) {
        return static_cast<double>(_fmtCtx->duration) / AV_TIME_BASE;
    }

    if (_videoStreamIndex >= 0) {
        AVStream* vs = _fmtCtx->streams[_videoStreamIndex];
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

void Decoder::startDecoding() {
    AVPacket* packet = av_packet_alloc();
    AVFrame*  vfr    = av_frame_alloc();
    AVFrame*  afr    = av_frame_alloc();

    const size_t MAX_QUEUE_SIZE = 50;
    bool firstAudioFrame = true;
    double audioStartPTS = 0.0;

    auto playbackStartTime = std::chrono::high_resolution_clock::now();

    while (_decodeRunning && av_read_frame(_fmtCtx, packet) >= 0) {
        // SEEK
        if (_seekRequested.load()) {
            double seekTime = _seekTarget.load();
            int64_t seekTimestamp = static_cast<int64_t>(seekTime * AV_TIME_BASE);
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
                    af.channels   = 2;
                    af.samples    = afr->nb_samples;
                    af.pts = (afr->best_effort_timestamp == AV_NOPTS_VALUE) ? 0.0
                                                                            : afr->best_effort_timestamp * av_q2d(_audioTimeBase);

                    if (firstAudioFrame) {
                        audioStartPTS = af.pts;
                        firstAudioFrame = false;
                        playbackStartTime = std::chrono::high_resolution_clock::now();
                    }

                    int outSamples = swr_get_out_samples(_swrCtx, afr->nb_samples);
                    int outBytes = av_samples_get_buffer_size(nullptr, af.channels, outSamples, AV_SAMPLE_FMT_S16, 1);
                    af.data.resize(outBytes);
                    uint8_t* outData[1] = { af.data.data() };
                    int convertedSamples = swr_convert(_swrCtx, outData, outSamples, (const uint8_t**)afr->data, afr->nb_samples);

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
                    vf.width  = vfr->width;
                    vf.height = vfr->height;
                    vf.pts = (vfr->best_effort_timestamp == AV_NOPTS_VALUE) ? 0.0
                                                                            : vfr->best_effort_timestamp * av_q2d(_videoTimeBase);

                    vf.data.resize(vf.width * vf.height * 3);
                    uint8_t* dest[1] = { vf.data.data() };
                    int lines[1] = { 3 * vf.width };
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