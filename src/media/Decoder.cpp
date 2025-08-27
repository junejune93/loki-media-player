#include "Decoder.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>
#include "AudioPlayer.h"
#include "VideoRenderer.h"

Decoder::Decoder(std::string file, const DecoderConfig &config) : filename(std::move(file)), _config(config) {
    initializeFFmpeg();

    openInputFile();

    findStreams();

    scanFrameTypes();

    initializeVideoDecoder();

    initializeAudioDecoder();

    initializeCodecInfo();
}

Decoder::~Decoder() { cleanup(); }

void Decoder::initializeFFmpeg() { avformat_network_init(); }

void Decoder::openInputFile() {
    const auto result = avformat_open_input(&_fmtCtx, filename.c_str(), nullptr, nullptr);
    if (result < 0) {
        throw std::runtime_error("Failed to open input file");
    }

    const auto streamInfoResult = avformat_find_stream_info(_fmtCtx, nullptr);
    if (streamInfoResult < 0) {
        throw std::runtime_error("Failed to get stream info");
    }
}

void Decoder::findStreams() {
    for (unsigned i = 0; i < _fmtCtx->nb_streams; ++i) {
        const auto *codecParams = _fmtCtx->streams[i]->codecpar;
        if (codecParams->codec_type == AVMEDIA_TYPE_VIDEO && _videoStreamIndex < 0) {
            _videoStreamIndex = static_cast<int>(i);
        } else if (codecParams->codec_type == AVMEDIA_TYPE_AUDIO && _audioStreamIndex < 0) {
            _audioStreamIndex = static_cast<int>(i);
        }
    }

    if (_videoStreamIndex < 0) {
        throw std::runtime_error("No video stream found");
    }
}

void Decoder::scanFrameTypes() {
    if (_videoStreamIndex < 0) {
        return;
    }

    scanForFrameTypes(_fmtCtx, _videoStreamIndex, _iFrameTimestamps);
}

void Decoder::initializeVideoDecoder() {
    if (_videoStreamIndex < 0) {
        return;
    }

    auto *videoStream = _fmtCtx->streams[_videoStreamIndex];
    const auto *videoCodec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!videoCodec) {
        throw std::runtime_error("Video codec not found");
    }

    _videoCtx = avcodec_alloc_context3(videoCodec);
    if (!_videoCtx) {
        throw std::runtime_error("Failed to allocate video codec context");
    }

    avcodec_parameters_to_context(_videoCtx, videoStream->codecpar);
    const auto openResult = avcodec_open2(_videoCtx, videoCodec, nullptr);
    if (openResult < 0) {
        throw std::runtime_error("Failed to open video codec");
    }

    _videoTimeBase = videoStream->time_base;

    initializeVideoScaler();
}

void Decoder::initializeVideoScaler() {
    _swsCtx = sws_getContext(_videoCtx->width, _videoCtx->height, _videoCtx->pix_fmt, _videoCtx->width,
                             _videoCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!_swsCtx) {
        throw std::runtime_error("Failed to create video scaler context");
    }
}

void Decoder::initializeAudioDecoder() {
    if (_audioStreamIndex < 0) {
        return;
    }

    auto *audioStream = _fmtCtx->streams[_audioStreamIndex];
    const auto *audioCodec = avcodec_find_decoder(audioStream->codecpar->codec_id);
    if (!audioCodec) {
        throw std::runtime_error("Audio codec not found");
    }

    _audioCtx = avcodec_alloc_context3(audioCodec);
    if (!_audioCtx) {
        throw std::runtime_error("Failed to allocate audio codec context");
    }

    avcodec_parameters_to_context(_audioCtx, audioStream->codecpar);
    const auto openResult = avcodec_open2(_audioCtx, audioCodec, nullptr);
    if (openResult < 0) {
        char error[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_make_error_string(error, AV_ERROR_MAX_STRING_SIZE, openResult);
        throw std::runtime_error("Failed to open audio codec: " + std::string(error));
    }

    _audioTimeBase = audioStream->time_base;

    initializeAudioResampler(audioStream);
}

void Decoder::initializeAudioResampler(AVStream *audioStream) {
    uint64_t inputChannelLayout = audioStream->codecpar->channel_layout;
    if (inputChannelLayout == 0) {
        inputChannelLayout = av_get_default_channel_layout(audioStream->codecpar->channels);
    }

    _swrCtx = swr_alloc();
    if (!_swrCtx) {
        throw std::runtime_error("Failed to allocate audio resampler");
    }

    const auto configOptions = [&]() {
        av_opt_set_int(_swrCtx, "in_channel_layout", static_cast<long>(inputChannelLayout), 0);
        av_opt_set_int(_swrCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
        av_opt_set_int(_swrCtx, "in_sample_rate", _audioCtx->sample_rate, 0);
        av_opt_set_int(_swrCtx, "out_sample_rate", _audioCtx->sample_rate, 0);
        av_opt_set_sample_fmt(_swrCtx, "in_sample_fmt", _audioCtx->sample_fmt, 0);
        av_opt_set_sample_fmt(_swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    };

    configOptions();

    const auto initResult = swr_init(_swrCtx);
    if (initResult < 0) {
        std::runtime_error("Failed to initialize audio resampler");
    }
}

void Decoder::initializeCodecInfo() {
    if (!_fmtCtx) {
        return;
    }

    _codecInfo = CodecInfo{};

    extractContainerFormat();
    extractVideoInfo();
    extractAudioInfo();
}

void Decoder::extractContainerFormat() {
    if (!_fmtCtx->iformat || !_fmtCtx->iformat->name) {
        return;
    }

    const std::string formatName = _fmtCtx->iformat->name;

    if (formatName == "mov,mp4,m4a,3gp,3g2,mj2") {
        _codecInfo.containerFormat = "MP4";
    } else if (formatName == "matroska,webm") {
        _codecInfo.containerFormat = "MKV/WebM";
    } else if (formatName == "avi") {
        _codecInfo.containerFormat = "AVI";
    } else {
        _codecInfo.containerFormat = formatName;
    }
}

void Decoder::extractVideoInfo() {
    if (_videoStreamIndex < 0 || !_videoCtx) {
        return;
    }

    _codecInfo.hasVideo = true;
    auto *videoStream = _fmtCtx->streams[_videoStreamIndex];

    if (_videoCtx->codec && _videoCtx->codec->name) {
        _codecInfo.videoCodec = normalizeVideoCodecName(_videoCtx->codec->name);
    }

    if (_videoCtx->width > 0 && _videoCtx->height > 0) {
        _codecInfo.videoResolution = std::to_string(_videoCtx->width) + "x" + std::to_string(_videoCtx->height);
    }

    if (videoStream->codecpar->bit_rate > 0) {
        _codecInfo.videoBitrate = CodecInfo::formatBitrate(videoStream->codecpar->bit_rate);
    } else if (_fmtCtx->bit_rate > 0) {
        const auto estimatedBitrate = static_cast<int64_t>(static_cast<double>(_fmtCtx->bit_rate) * 0.8);
        _codecInfo.videoBitrate = CodecInfo::formatBitrate(estimatedBitrate);
    }
}

void Decoder::extractAudioInfo() {
    if (_audioStreamIndex < 0 || !_audioCtx) {
        return;
    }

    _codecInfo.hasAudio = true;
    auto *audioStream = _fmtCtx->streams[_audioStreamIndex];

    if (_audioCtx->codec && _audioCtx->codec->name) {
        _codecInfo.audioCodec = normalizeAudioCodecName(_audioCtx->codec->name);
    }

    if (_audioCtx->sample_rate > 0) {
        _codecInfo.audioSampleRate = CodecInfo::formatSampleRate(_audioCtx->sample_rate);
    }

    if (audioStream->codecpar->bit_rate > 0) {
        _codecInfo.audioBitrate = CodecInfo::formatBitrate(audioStream->codecpar->bit_rate);
    }

    if (_audioCtx->channels > 0) {
        uint64_t channelLayout = audioStream->codecpar->channel_layout;
        if (channelLayout == 0) {
            channelLayout = av_get_default_channel_layout(_audioCtx->channels);
        }
        _codecInfo.audioChannels = CodecInfo::formatChannelLayout(_audioCtx->channels, channelLayout);
    }
}

std::string Decoder::normalizeVideoCodecName(const std::string &codecName) {
    static const std::unordered_map<std::string, std::string> codecMap = {
            {"h264", "H.264/AVC"}, {"hevc", "H.265/HEVC"}, {"vp9", "VP9"}, {"vp8", "VP8"}, {"av1", "AV1"}};

    const auto it = codecMap.find(codecName);
    return (it != codecMap.end()) ? it->second : "UNKNOWN";
}

std::string Decoder::normalizeAudioCodecName(const std::string &codecName) {
    static const std::unordered_map<std::string, std::string> codecMap = {
            {"aac", "AAC"}, {"mp3", "MP3"},   {"ac3", "AC-3"},     {"eac3", "E-AC-3"},
            {"dts", "DTS"}, {"opus", "Opus"}, {"vorbis", "Vorbis"}};

    const auto it = codecMap.find(codecName);
    return (it != codecMap.end()) ? it->second : "UNKNOWN";
}

void Decoder::scanForFrameTypes(AVFormatContext *fmtCtx, int videoStreamIndex, std::vector<double> &timestamps) {
    av_seek_frame(fmtCtx, videoStreamIndex, 0, AVSEEK_FLAG_FRAME);

    auto packet_deleter = [](AVPacket *p) { av_packet_free(&p); };
    std::unique_ptr<AVPacket, decltype(packet_deleter)> packet(av_packet_alloc(), packet_deleter);

    clearFrameTimestamps();

    while (av_read_frame(fmtCtx, packet.get()) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            const double pts = static_cast<double>(packet->pts) * av_q2d(fmtCtx->streams[videoStreamIndex]->time_base);

            if (packet->flags & AV_PKT_FLAG_KEY) {
                addIFrameTimestamp(pts);
            } else if (!(packet->flags & AV_PKT_FLAG_DISCARD)) {
                addPFrameTimestamp(pts);
            }
        }
        av_packet_unref(packet.get());
    }

    sortFrameTimestamps();

    {
        std::lock_guard<std::mutex> lock(_iFrameTimestampsMutex);
        timestamps = _iFrameTimestamps;
    }

    av_seek_frame(fmtCtx, videoStreamIndex, 0, AVSEEK_FLAG_FRAME);
}

void Decoder::clearFrameTimestamps() {
    {
        std::lock_guard<std::mutex> lock(_iFrameTimestampsMutex);
        _iFrameTimestamps.clear();
    }

    {
        std::lock_guard<std::mutex> lock(_pFrameTimestampsMutex);
        _pFrameTimestamps.clear();
    }
}

void Decoder::addIFrameTimestamp(double pts) {
    std::lock_guard<std::mutex> lock(_iFrameTimestampsMutex);
    _iFrameTimestamps.push_back(pts);
}

void Decoder::addPFrameTimestamp(double pts) {
    std::lock_guard<std::mutex> lock(_pFrameTimestampsMutex);
    _pFrameTimestamps.push_back(pts);
}

void Decoder::sortFrameTimestamps() {
    {
        std::lock_guard<std::mutex> lock(_iFrameTimestampsMutex);
        std::sort(_iFrameTimestamps.begin(), _iFrameTimestamps.end());
    }

    {
        std::lock_guard<std::mutex> lock(_pFrameTimestampsMutex);
        std::sort(_pFrameTimestamps.begin(), _pFrameTimestamps.end());
    }
}

CodecInfo Decoder::getCodecInfo() const { return _codecInfo; }

void Decoder::start() {
    if (_decodeRunning.load()) {
        return;
    }

    _decodeRunning = true;
    _decodeThread = std::thread(&Decoder::startDecoding, this);
}

void Decoder::stop() {
    if (!_decodeRunning.load()) {
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
        auto *videoStream = _fmtCtx->streams[_videoStreamIndex];
        if (videoStream->duration != AV_NOPTS_VALUE) {
            return static_cast<double>(videoStream->duration) * av_q2d(videoStream->time_base);
        }
    }

    return 0.0;
}

bool Decoder::seek(double timeInSeconds) {
    if (!_fmtCtx || !_decodeRunning.load()) {
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
    auto packet_deleter = [](AVPacket *p) { av_packet_free(&p); };
    std::unique_ptr<AVPacket, decltype(packet_deleter)> packet(av_packet_alloc(), packet_deleter);

    auto frame_deleter = [](AVFrame *f) { av_frame_free(&f); };
    std::unique_ptr<AVFrame, decltype(frame_deleter)> videoFrame(av_frame_alloc(), frame_deleter);
    std::unique_ptr<AVFrame, decltype(frame_deleter)> audioFrame(av_frame_alloc(), frame_deleter);

    DecodingState state;

    while (_decodeRunning.load() && av_read_frame(_fmtCtx, packet.get()) >= 0) {
        if (handleSeekRequest(state)) {
            av_packet_unref(packet.get());
            continue;
        }

        if (!waitForQueueSpace()) {
            break;
        }

        if (_audioCtx && packet->stream_index == _audioStreamIndex) {
            decodeAudioPacket(packet.get(), audioFrame.get(), state);
        } else if (packet->stream_index == _videoStreamIndex) {
            decodeVideoPacket(packet.get(), videoFrame.get(), state);
        }

        av_packet_unref(packet.get());
    }
}

bool Decoder::handleSeekRequest(DecodingState &state) {
    if (!_seekRequested.load()) {
        return false;
    }

    const double seekTime = _seekTarget.load();
    const auto seekTimestamp = static_cast<int64_t>(seekTime * AV_TIME_BASE);

    if (av_seek_frame(_fmtCtx, -1, seekTimestamp, AVSEEK_FLAG_BACKWARD) >= 0) {
        if (_videoCtx) {
            avcodec_flush_buffers(_videoCtx);
        }

        if (_audioCtx) {
            avcodec_flush_buffers(_audioCtx);
        }

        state.reset();
    }

    _seekRequested = false;
    return true;
}

bool Decoder::waitForQueueSpace() const {
    while (_decodeRunning.load() && (_videoQueue.size() > MAX_QUEUE_SIZE || _audioQueue.size() > MAX_QUEUE_SIZE)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return _decodeRunning.load();
}

void Decoder::decodeAudioPacket(AVPacket *packet, AVFrame *frame, DecodingState &state) {
    if (avcodec_send_packet(_audioCtx, packet) < 0) {
        return;
    }

    while (avcodec_receive_frame(_audioCtx, frame) >= 0) {
        auto audioFrame = createAudioFrame(frame, state);
        if (audioFrame.has_value()) {
            _audioQueue.push(std::move(*audioFrame));
        }
    }
}

void Decoder::decodeVideoPacket(AVPacket *packet, AVFrame *frame, DecodingState &state) {
    if (avcodec_send_packet(_videoCtx, packet) < 0) {
        return;
    }

    while (avcodec_receive_frame(_videoCtx, frame) >= 0) {
        auto videoFrame = createVideoFrame(frame, state);
        if (videoFrame.has_value()) {
            syncVideoFrame(*videoFrame, state);
            _videoQueue.push(std::move(*videoFrame));
        }
    }
}

std::optional<AudioFrame> Decoder::createAudioFrame(AVFrame *frame, DecodingState &state) {
    AudioFrame audioFrame;
    audioFrame.sampleRate = _audioCtx->sample_rate;
    audioFrame.channels = 2;
    audioFrame.samples = frame->nb_samples;
    audioFrame.pts = (frame->best_effort_timestamp == AV_NOPTS_VALUE)
                             ? 0.0
                             : static_cast<double>(frame->best_effort_timestamp) * av_q2d(_audioTimeBase);

    if (state.isFirstAudioFrame) {
        state.audioStartPTS = audioFrame.pts;
        state.isFirstAudioFrame = false;
        state.playbackStartTime = std::chrono::high_resolution_clock::now();
    }

    const int outSamples = swr_get_out_samples(_swrCtx, frame->nb_samples);
    const int outBytes = av_samples_get_buffer_size(nullptr, audioFrame.channels, outSamples, AV_SAMPLE_FMT_S16, 1);

    audioFrame.data.resize(outBytes);
    uint8_t *outData[1] = {audioFrame.data.data()};

    const int convertedSamples =
            swr_convert(_swrCtx, outData, outSamples, const_cast<const uint8_t **>(frame->data), frame->nb_samples);

    if (convertedSamples <= 0) {
        return std::nullopt;
    }

    audioFrame.data.resize(convertedSamples * audioFrame.channels * sizeof(int16_t));
    audioFrame.samples = convertedSamples;

    return audioFrame;
}

std::optional<VideoFrame> Decoder::createVideoFrame(AVFrame *frame, const DecodingState &state) {
    VideoFrame videoFrame;
    videoFrame.width = frame->width;
    videoFrame.height = frame->height;
    videoFrame.pts = (frame->best_effort_timestamp == AV_NOPTS_VALUE)
                             ? 0.0
                             : static_cast<double>(frame->best_effort_timestamp) * av_q2d(_videoTimeBase);

    videoFrame.data.resize(videoFrame.width * videoFrame.height * 3);
    uint8_t *dest[1] = {videoFrame.data.data()};
    int lines[1] = {3 * videoFrame.width};

    sws_scale(_swsCtx, frame->data, frame->linesize, 0, videoFrame.height, dest, lines);

    return videoFrame;
}

void Decoder::syncVideoFrame(const VideoFrame &videoFrame, const DecodingState &state) {
    if (state.isFirstAudioFrame) {
        return;
    }

    const double relativeVideoPTS = videoFrame.pts - state.audioStartPTS;
    const auto now = std::chrono::high_resolution_clock::now();
    const double elapsed = std::chrono::duration<double>(now - state.playbackStartTime).count();

    if (relativeVideoPTS > elapsed) {
        const auto sleepDuration = std::chrono::duration<double>(relativeVideoPTS - elapsed);
        std::this_thread::sleep_for(sleepDuration);
    }
}

void Decoder::flush() {
    if (_videoCtx) {
        avcodec_flush_buffers(_videoCtx);
    }

    if (_audioCtx) {
        avcodec_flush_buffers(_audioCtx);
    }
}

void Decoder::cleanup() {
    stop();

    if (_swsCtx) {
        sws_freeContext(_swsCtx);
        _swsCtx = nullptr;
    }

    if (_videoCtx) {
        avcodec_free_context(&_videoCtx);
    }

    if (_audioCtx) {
        avcodec_free_context(&_audioCtx);
    }

    if (_swrCtx) {
        swr_free(&_swrCtx);
    }

    if (_fmtCtx) {
        avformat_close_input(&_fmtCtx);
    }

    avformat_network_deinit();
}
