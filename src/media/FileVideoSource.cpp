#include "FileVideoSource.h"

FileVideoSource::FileVideoSource(const std::string &filename)
        : _decoder(std::make_unique<Decoder>(filename)) {

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