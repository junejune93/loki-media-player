#include "FileVideoSource.h"
#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>

FileVideoSource::FileVideoSource(const std::string &filename)
    : _decoder(std::make_unique<Decoder>(filename)) {
    _outputDir = "record";
    std::filesystem::create_directories(_outputDir);
}

FileVideoSource::~FileVideoSource() = default;

void FileVideoSource::start() {
    _decoder->start();
}

void FileVideoSource::stop() {
    _decoder->stop();
}

void FileVideoSource::flush() {
    _decoder->flush();
}

bool FileVideoSource::seek(double timeInSeconds) {
    return _decoder->seek(timeInSeconds);
}

double FileVideoSource::getDuration() const {
    return _decoder->getDuration();
}

CodecInfo FileVideoSource::getCodecInfo() const {
    return _decoder->getCodecInfo();
}

ThreadSafeQueue<VideoFrame> &FileVideoSource::getVideoQueue() {
    return _decoder->getVideoQueue();
}

ThreadSafeQueue<AudioFrame> &FileVideoSource::getAudioQueue() {
    return _decoder->getAudioQueue();
}

Decoder &FileVideoSource::decoder() {
    return *_decoder;
}

void FileVideoSource::startRecord() {
    if (_isRecording) {
        return;
    }

    try {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&now_time);

        std::ostringstream ss;
        ss << _outputDir << "/recording_" << std::put_time(&tm, "%Y%m%d_%H%M%S");
        std::string sessionDir = ss.str();

        if (!std::filesystem::create_directories(sessionDir)) {
            throw std::runtime_error("Failed to create recording directory: " + sessionDir);
        }

        auto codecInfo = _decoder->getCodecInfo();
        if (!codecInfo.hasVideo) {
            throw std::runtime_error("No video stream available for recording");
        }

        size_t xPos = codecInfo.videoResolution.find('x');
        if (xPos == std::string::npos) {
            throw std::runtime_error("Invalid video resolution format");
        }

        int width = std::stoi(codecInfo.videoResolution.substr(0, xPos));
        int height = std::stoi(codecInfo.videoResolution.substr(xPos + 1));

        int fps = 30;

        _encoder = std::make_unique<Encoder>(sessionDir, 180, Encoder::OutputFormat::MP4);
        if (!_encoder->initialize(
                width,
                height,
                fps,
                AV_PIX_FMT_RGBA)) {
            throw std::runtime_error("Failed to initialize encoder");
        }

        _isRecording = true;
        spdlog::info("Started recording to: {}", sessionDir);
    } catch (const std::exception &e) {
        _encoder.reset();
        _isRecording = false;
        spdlog::error("Failed to start recording: {}", e.what());
        throw;
    }
}

void FileVideoSource::stopRecord() {
    if (!_isRecording) {
        spdlog::warn("No active recording to stop");
        return;
    }

    try {
        _isRecording = false;

        if (_encoder) {
            try {
                _encoder->finalize();
            } catch (const std::exception& e) {
                spdlog::warn("Error finalizing encoder: {}", e.what());
            }
            _encoder.reset();
        }

        spdlog::info("Recording stopped and saved");
    } catch (const std::exception &e) {
        spdlog::error("Error while stopping recording: {}", e.what());
        _encoder.reset();
        _isRecording = false;
        throw;
    }
}

void FileVideoSource::encodeFrame(const VideoFrame &frame) {
    if (!_isRecording || !_encoder) {
        return;
    }

    try {
        VideoFrame frameCopy = frame;

        if (frameCopy.width <= 0 || frameCopy.height <= 0) {
            return;
        }

        if (frameCopy.data.empty()) {
            return;
        }

        size_t rgb_size = frameCopy.width * frameCopy.height * 3;
        size_t rgba_size = frameCopy.width * frameCopy.height * 4;
        size_t yuv420p_size = frameCopy.width * frameCopy.height * 3 / 2;

        if (frameCopy.data.size() != rgb_size && frameCopy.data.size() != rgba_size &&
            frameCopy.data.size() != yuv420p_size) {
            return;
        }

        std::string formatName = "";
        if (frameCopy.data.size() == rgba_size) {
            formatName = "RGBA";
        } else if (frameCopy.data.size() == rgb_size) {
            formatName = "RGB";
        } else if (frameCopy.data.size() == yuv420p_size) {
            formatName = "YUV420P";
        } else {
            formatName = "UNKNOWN";
        }
        _encoder->encodeFrame(frameCopy);
    } catch (const std::exception &e) {
    }
}

std::vector<double> FileVideoSource::getIFrameTimestamps() const {
    if (!_decoder) {
        spdlog::info("No decoder available for I-Frame timestamps");
        return {};
    }
    auto timestamps = _decoder->getIFrameTimestamps();
    spdlog::info("Retrieved {} I-Frame timestamps", timestamps.size());
    if (!timestamps.empty()) {
        spdlog::info("First I-Frame at: {}s", timestamps[0]);
        spdlog::info("Last I-Frame at: {}s", timestamps.back());
    }
    return timestamps;
}

std::vector<double> FileVideoSource::getPFrameTimestamps() const {
    if (!_decoder) {
        spdlog::info("No decoder available for P-Frame timestamps");
        return {};
    }
    auto timestamps = _decoder->getPFrameTimestamps();
    spdlog::info("Retrieved {} P-Frame timestamps", timestamps.size());
    if (!timestamps.empty()) {
        spdlog::info("First P-Frame at: {}s", timestamps[0]);
        spdlog::info("Last P-Frame at: {}s", timestamps.back());
    }
    return timestamps;
}