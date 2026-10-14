#include "webmdx/SourceDecoder.h"
#include <algorithm>
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

    DemuxResult SourceDecoder::EnsureHeader() const {
        return _impl->Prepare();
    }

    bool SourceDecoder::IsReady() const {
        return _impl->parseState == Impl::ParseState::Ready;
    }

    DemuxResult SourceDecoder::Demux(double seconds) const {
        const auto result = _impl->Decode(seconds);
        _impl->ReleaseConsumed();
        return result;
    }

    DemuxResult SourceDecoder::Seek(double seconds) const {
        const auto result = _impl->Seek(seconds);
        _impl->ReleaseConsumed();
        return result;
    }

    ByteRange SourceDecoder::GetNeededRange() const {
        return _impl->bytesNeeded > 0 ? ByteRange{_impl->neededStart, _impl->bytesNeeded} : ByteRange{};
    }

    std::int64_t SourceDecoder::GetBytesNeeded() const {
        return _impl->bytesNeeded;
    }

    void SourceDecoder::SetMaxHeaderSize(const std::int64_t bytes) const {
        _impl->maxHeaderSize = bytes;
    }

    std::int64_t SourceDecoder::GetMaxHeaderSize() const {
        return _impl->maxHeaderSize;
    }

    double SourceDecoder::GetDuration() const {
        return std::max(0.0, _impl->duration);
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

    void SourceDecoder::SetVideoDecoderFactory(const VideoDecoderFactory &factory) const {
        _impl->videoDecoderFactory = factory ? factory : VideoDecoderFactory{IVideoDecoder::Create};
    }

    void SourceDecoder::SetAudioDecoderFactory(const AudioDecoderFactory &factory) const {
        _impl->audioDecoderFactory = factory ? factory : AudioDecoderFactory{IAudioDecoder::Create};
    }

    void SourceDecoder::SetVideoPacketCallback(const VideoPacketCallback& callback) const
    {
        _impl->_videoPacketCallback = callback;
    }
}
