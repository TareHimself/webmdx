#pragma once
#include <functional>
#include <memory>
#include <span>

#include "AudioTrack.h"
#include "DemuxResult.h"
#include "IAudioDecoder.h"
#include "IDecodedVideoFrame.h"
#include "ISource.h"
#include "IVideoDecoder.h"
#include "Packet.h"
#include "VideoTrack.h"

namespace wdx {
    class WEBMDX_API SourceDecoder {
    public:
        using VideoCallback = std::function<void(double, const std::shared_ptr<IDecodedVideoFrame> &)>;
        using AudioCallback = std::function<void(double, const std::span<float> &)>;

        using VideoPacketCallback = std::function<void(const std::shared_ptr<Packet> &, IVideoDecoder * decoder)>;
        using AudioPacketCallback = std::function<void(const std::shared_ptr<Packet> &, IAudioDecoder * decoder)>;
        struct Impl;

        SourceDecoder();

        ~SourceDecoder();

        void SetSource(const std::shared_ptr<ISource> &source) const;

        [[nodiscard]] DemuxResult Demux(double seconds) const;

        void Seek(double seconds) const;

        [[nodiscard]] double GetDuration() const;

        [[nodiscard]] double GetPosition() const;

        [[nodiscard]] bool HasAudio() const;

        [[nodiscard]] bool HasVideo() const;

        [[nodiscard]] int GetAudioTrackCount() const;

        [[nodiscard]] int GetVideoTrackCount() const;

        [[nodiscard]] AudioTrack GetAudioTrack(int index) const;

        [[nodiscard]] VideoTrack GetVideoTrack(int index) const;

        [[nodiscard]] AudioTrack GetAudioTrack() const;

        [[nodiscard]] VideoTrack GetVideoTrack() const;

        [[nodiscard]] IAudioDecoder * GetAudioDecoder() const;

        [[nodiscard]] IVideoDecoder * GetVideoDecoder() const;

        void SetAudioPacketCallback(const AudioPacketCallback &callback) const;

        void SetVideoPacketCallback(const VideoPacketCallback &callback) const;

    private:
        Impl *_impl;
    };
}
