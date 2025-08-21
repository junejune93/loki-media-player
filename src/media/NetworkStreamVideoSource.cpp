#include "NetworkStreamVideoSource.h"

NetworkStreamVideoSource::NetworkStreamVideoSource(const std::string &uri)
        : _decoder(std::make_unique<Decoder>(uri)) {

}

NetworkStreamVideoSource::~NetworkStreamVideoSource() = default;

void NetworkStreamVideoSource::start() {
    _decoder->start();
}

void NetworkStreamVideoSource::stop() {
    _decoder->stop();
}

void NetworkStreamVideoSource::flush() {
    _decoder->flush();
}

bool NetworkStreamVideoSource::seek(double t) {
    return _decoder->seek(t);
}

double NetworkStreamVideoSource::getDuration() const {
    return _decoder->getDuration();
}

CodecInfo NetworkStreamVideoSource::getCodecInfo() const {
    return _decoder->getCodecInfo();
}

ThreadSafeQueue<VideoFrame> &NetworkStreamVideoSource::getVideoQueue() {
    return _decoder->getVideoQueue();
}

ThreadSafeQueue<AudioFrame> &NetworkStreamVideoSource::getAudioQueue() {
    return _decoder->getAudioQueue();
}