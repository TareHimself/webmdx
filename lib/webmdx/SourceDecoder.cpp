#include "webmdx/SourceDecoder.h"
#include <filesystem>
#include "SourceDecoderImpl.h"

namespace wdx {
    SourceDecoder::SourceDecoder() {
        _impl = new Impl();
    }

    SourceDecoder::~SourceDecoder() {
        delete _impl;
        _impl = nullptr;
    }

    void SourceDecoder::SetSource(const std::shared_ptr<ISource> &source) const {
        _impl->SetSource(source);
    }

    DecodeResult SourceDecoder::Decode(double seconds) const {
        return _impl->Decode(seconds);
    }

    void SourceDecoder::Seek(double seconds) const {
        _impl->Seek(seconds);
    }

    double SourceDecoder::GetDuration() const {
        return _impl->duration;
    }

    double SourceDecoder::GetPosition() const {
        return _impl->decodedPosition;
    }

    bool SourceDecoder::HasAudio() const {
        return GetAudioTrackCount() > 0;
    }

    bool SourceDecoder::HasVideo() const {
        return GetVideoTrackCount() > 0;
    }

    int SourceDecoder::GetAudioTrackCount() const {
        return _impl->audioTracks.size();
    }

    int SourceDecoder::GetVideoTrackCount() const {
        return _impl->videoTracks.size();
    }

    AudioTrack SourceDecoder::GetAudioTrack(int index) const {
        return _impl->audioTracks[index];
    }

    VideoTrack SourceDecoder::GetVideoTrack(int index) const {
        return _impl->videoTracks[index];
    }

    AudioTrack SourceDecoder::GetAudioTrack() const {
        return _impl->audioTracks[_impl->selectedAudioTrackIndex];
    }

    VideoTrack SourceDecoder::GetVideoTrack() const {
        return _impl->videoTracks[_impl->selectedVideoTrackIndex];
    }

    void SourceDecoder::SetVideoCallback(const VideoCallback &callback) {
        _impl->_videoCallback = callback;
    }

    void SourceDecoder::SetAudioCallback(const AudioCallback &callback) {
        _impl->_audioCallback = callback;
    }
}
