#include "FileVideoSource.h"
#include <filesystem>
#include <iostream>

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
        std::cout << "[INFO] Started recording to: " << sessionDir << std::endl;
    } catch (const std::exception &e) {
        _encoder.reset();
        _isRecording = false;
        std::cerr << "[ERROR] Failed to start recording: " << e.what() << std::endl;
        throw;
    }
}

void FileVideoSource::stopRecord() {
    if (!_isRecording) {
        std::cerr << "[WARNING] No active recording to stop" << std::endl;
        return;
    }

    try {
        _isRecording = false;

        if (_encoder) {
            try {
                _encoder->finalize();
            } catch (const std::exception& e) {
                std::cerr << "[WARNING] Error finalizing encoder: " << e.what() << std::endl;
            }
            _encoder.reset();
        }

        std::cout << "[INFO] Recording stopped and saved" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] Error while stopping recording: " << e.what() << std::endl;
        _encoder.reset();
        _isRecording = false;
        throw;
    }
}