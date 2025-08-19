#include "Decoder.h"
#include "VideoRenderer.h"
#include "AudioPlayer.h"
#include <iostream>
#include <stdexcept>

Decoder::Decoder(const std::string& file) : filename(file) {
    avformat_network_init();

    if (avformat_open_input(&fmtCtx, filename.c_str(), nullptr, nullptr) != 0)
        throw std::runtime_error("Failed to open input file");

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
        throw std::runtime_error("Failed to get _stream info");

    // Find streams
    for (unsigned i = 0; i < fmtCtx->nb_streams; ++i) {
        AVCodecParameters* par = fmtCtx->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex < 0) {
            videoStreamIndex = static_cast<int>(i);
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0) {
            audioStreamIndex = static_cast<int>(i);
        }
    }
    if (videoStreamIndex < 0)
        throw std::runtime_error("No video _stream found");

    // Video codec
    {
        AVStream* vs = fmtCtx->streams[videoStreamIndex];
        const AVCodec* vcodec = avcodec_find_decoder(vs->codecpar->codec_id);
        if (!vcodec) throw std::runtime_error("Video codec not found");
        videoCtx = avcodec_alloc_context3(vcodec);
        avcodec_parameters_to_context(videoCtx, vs->codecpar);
        if (avcodec_open2(videoCtx, vcodec, nullptr) < 0)
            throw std::runtime_error("Failed to open video codec");
        videoTimeBase = vs->time_base;

        swsCtx = sws_getContext(
                videoCtx->width, videoCtx->height, videoCtx->pix_fmt,
                videoCtx->width, videoCtx->height, AV_PIX_FMT_RGB24,
                SWS_BILINEAR, nullptr, nullptr, nullptr
        );
    }

    // Audio codec (optional)
    if (audioStreamIndex >= 0) {
        AVStream* as = fmtCtx->streams[audioStreamIndex];
        const AVCodec* acodec = avcodec_find_decoder(as->codecpar->codec_id);
        if (!acodec) throw std::runtime_error("Audio codec not found");
        audioCtx = avcodec_alloc_context3(acodec);
        avcodec_parameters_to_context(audioCtx, as->codecpar);
        if (avcodec_open2(audioCtx, acodec, nullptr) < 0)
            throw std::runtime_error("Failed to open audio codec");
        audioTimeBase = as->time_base;

        // Swr: to S16 stereo @ input sample_rate
        uint64_t in_ch_layout = as->codecpar->channel_layout;
        if (in_ch_layout == 0)
            in_ch_layout = av_get_default_channel_layout(as->codecpar->channels);

        swrCtx = swr_alloc();
        if (!swrCtx) throw std::runtime_error("Failed to alloc swrCtx");

        av_opt_set_int(swrCtx, "in_channel_layout",  in_ch_layout, 0);
        av_opt_set_int(swrCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        av_opt_set_int(swrCtx, "in_sample_rate",  audioCtx->sample_rate, 0);
        av_opt_set_int(swrCtx, "out_sample_rate", audioCtx->sample_rate, 0);
        av_opt_set_sample_fmt(swrCtx, "in_sample_fmt",  audioCtx->sample_fmt, 0);
        av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

        if (swr_init(swrCtx) < 0)
            throw std::runtime_error("Failed to init swr");
    }
}

Decoder::~Decoder() {
    stop();
    if (swsCtx)   sws_freeContext(swsCtx);
    if (videoCtx) avcodec_free_context(&videoCtx);
    if (audioCtx) avcodec_free_context(&audioCtx);
    if (swrCtx)   swr_free(&swrCtx);
    if (fmtCtx)   avformat_close_input(&fmtCtx);
    avformat_network_deinit();
}

void Decoder::start() {
    if (running) return;
    running = true;
    decodeThread = std::thread(&Decoder::startDecoding, this);
}

void Decoder::stop() {
    if (!running) return;
    running = false;
    if (decodeThread.joinable()) decodeThread.join();
}

double Decoder::getDuration() const {
    if (!fmtCtx) {
        return 0.0;
    }

    if (fmtCtx->duration != AV_NOPTS_VALUE) {
        return static_cast<double>(fmtCtx->duration) / AV_TIME_BASE;
    }

    if (videoStreamIndex >= 0) {
        AVStream* vs = fmtCtx->streams[videoStreamIndex];
        if (vs->duration != AV_NOPTS_VALUE) {
            return static_cast<double>(vs->duration) * av_q2d(vs->time_base);
        }
    }

    return 0.0;
}

bool Decoder::seek(double timeInSeconds) {
    if (!fmtCtx || !running) {
        return false;
    }

    seekTarget = timeInSeconds;
    seekRequested = true;

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

    while (running && av_read_frame(fmtCtx, packet) >= 0) {
        // SEEK
        if (seekRequested.load()) {
            double seekTime = seekTarget.load();
            int64_t seekTimestamp = static_cast<int64_t>(seekTime * AV_TIME_BASE);
            if (av_seek_frame(fmtCtx, -1, seekTimestamp, AVSEEK_FLAG_BACKWARD) >= 0) {
                if (videoCtx) {
                    avcodec_flush_buffers(videoCtx);
                }
                if (audioCtx) {
                    avcodec_flush_buffers(audioCtx);
                }
                firstAudioFrame = true;
                audioStartPTS = 0.0;
                playbackStartTime = std::chrono::high_resolution_clock::now();
            }

            seekRequested = false;
            av_packet_unref(packet);
            continue;
        }

        while (running && (videoQueue.size() > MAX_QUEUE_SIZE || audioQueue.size() > MAX_QUEUE_SIZE))
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (!running) break;

        // ---------------- AUDIO ----------------
        if (audioCtx && packet->stream_index == audioStreamIndex) {
            if (avcodec_send_packet(audioCtx, packet) >= 0) {
                while (avcodec_receive_frame(audioCtx, afr) >= 0) {
                    AudioFrame af;
                    af.sampleRate = audioCtx->sample_rate;
                    af.channels   = 2;
                    af.samples    = afr->nb_samples;
                    af.pts = (afr->best_effort_timestamp == AV_NOPTS_VALUE) ? 0.0
                                                                            : afr->best_effort_timestamp * av_q2d(audioTimeBase);

                    if (firstAudioFrame) {
                        audioStartPTS = af.pts;
                        firstAudioFrame = false;
                        playbackStartTime = std::chrono::high_resolution_clock::now();
                    }

                    int outSamples = swr_get_out_samples(swrCtx, afr->nb_samples);
                    int outBytes = av_samples_get_buffer_size(nullptr, af.channels, outSamples, AV_SAMPLE_FMT_S16, 1);
                    af.data.resize(outBytes);
                    uint8_t* outData[1] = { af.data.data() };
                    int convertedSamples = swr_convert(swrCtx, outData, outSamples, (const uint8_t**)afr->data, afr->nb_samples);

                    if (convertedSamples > 0) {
                        af.data.resize(convertedSamples * af.channels * sizeof(int16_t));
                        af.samples = convertedSamples;
                        audioQueue.push(std::move(af));
                    }
                }
            }
        }

            // ---------------- VIDEO ----------------
        else if (packet->stream_index == videoStreamIndex) {
            if (avcodec_send_packet(videoCtx, packet) >= 0) {
                while (avcodec_receive_frame(videoCtx, vfr) >= 0) {
                    VideoFrame vf;
                    vf.width  = vfr->width;
                    vf.height = vfr->height;
                    vf.pts = (vfr->best_effort_timestamp == AV_NOPTS_VALUE) ? 0.0
                                                                            : vfr->best_effort_timestamp * av_q2d(videoTimeBase);

                    vf.data.resize(vf.width * vf.height * 3);
                    uint8_t* dest[1] = { vf.data.data() };
                    int lines[1] = { 3 * vf.width };
                    sws_scale(swsCtx, vfr->data, vfr->linesize, 0, vf.height, dest, lines);

                    if (!firstAudioFrame) {
                        double relativeVideoPTS = vf.pts - audioStartPTS;
                        auto now = std::chrono::high_resolution_clock::now();
                        double elapsed = std::chrono::duration<double>(now - playbackStartTime).count();

                        if (relativeVideoPTS > elapsed) {
                            std::this_thread::sleep_for(std::chrono::duration<double>(relativeVideoPTS - elapsed));
                        }
                    }

                    videoQueue.push(std::move(vf));
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
    if (videoCtx) {
        avcodec_flush_buffers(videoCtx);
    }

    if (audioCtx) {
        avcodec_flush_buffers(audioCtx);
    }
}