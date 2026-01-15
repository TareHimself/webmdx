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

    DemuxResult SourceDecoder::Demux(double seconds) const {
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

    IAudioDecoder* SourceDecoder::GetAudioDecoder() const
    {
        return _impl->GetAudioDecoder();
    }

    IVideoDecoder* SourceDecoder::GetVideoDecoder() const
    {
        return _impl->GetVideoDecoder();
    }

    void SourceDecoder::SetAudioPacketCallback(const AudioPacketCallback& callback) const
    {
        _impl->_audioPacketCallback = callback;
    }

    void SourceDecoder::SetVideoPacketCallback(const VideoPacketCallback& callback) const
    {
        _impl->_videoPacketCallback = callback;
    }
}
