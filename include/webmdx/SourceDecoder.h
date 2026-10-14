#pragma once
#include <functional>
#include <memory>
#include <span>

#include "AudioTrack.h"
#include "ByteRange.h"
#include "DemuxResult.h"
#include "IAudioDecoder.h"
#include "IDecodedVideoFrame.h"
#include "ISource.h"
#include "IVideoDecoder.h"
#include "Packet.h"
#include "VideoTrack.h"

namespace wdx {
    // Demuxes a WebM file from an ISource that may only partly be there:
    //
    //     decoder.SetSource(source);
    //     for (;;) {
    //         const auto result = decoder.Demux(0.1);
    //         if (result == DemuxResult::Finished) break;
    //         if (result == DemuxResult::Success) continue;
    //         // Out of data: feed the source up to GetBytesNeeded(), or end it.
    //     }
    class WEBMDX_API SourceDecoder {
    public:
        using VideoCallback = std::function<void(double, const std::shared_ptr<IDecodedVideoFrame> &)>;
        using AudioCallback = std::function<void(double, const std::span<float> &)>;

        using VideoPacketCallback = std::function<void(const std::shared_ptr<Packet> &, IVideoDecoder * decoder)>;
        using AudioPacketCallback = std::function<void(const std::shared_ptr<Packet> &, IAudioDecoder * decoder)>;

        using VideoDecoderFactory = std::function<std::shared_ptr<IVideoDecoder>(const VideoTrack &)>;
        using AudioDecoderFactory = std::function<std::shared_ptr<IAudioDecoder>(const AudioTrack &)>;
        struct Impl;

        SourceDecoder();

        ~SourceDecoder();

        // Reads nothing. Tracks, duration and decoders are unknown until IsReady.
        void SetSource(const std::shared_ptr<ISource> &source) const;

        // Reads the headers without demuxing. Demux and Seek do this themselves.
        DemuxResult EnsureHeader() const;

        // True once the headers have been read.
        [[nodiscard]] bool IsReady() const;

        // Advances by seconds, calling the packet callbacks. Call again after an Incomplete result, nothing is lost.
        // Throws HeaderDecodeFailedException or SourceDecodeException on bad data.
        [[nodiscard]] DemuxResult Demux(double seconds) const;

        // Changes nothing unless it returns Success. Video restarts from the key frame before seconds.
        // Jumps there through the file's index when it has one and the source holds or can fetch that part.
        DemuxResult Seek(double seconds) const;

        // After an Incomplete result, the bytes the source is missing for the next step. More may be needed after
        // that. A source that only grows from its start needs everything up to end. Empty when nothing is needed.
        [[nodiscard]] ByteRange GetNeededRange() const;

        // The end of GetNeededRange.
        [[nodiscard]] std::int64_t GetBytesNeeded() const;

        // Headers that would reach past this many bytes throw HeaderDecodeFailedException. Default 4 MiB, 0 for no limit.
        void SetMaxHeaderSize(std::int64_t bytes) const;

        [[nodiscard]] std::int64_t GetMaxHeaderSize() const;

        // Seconds. 0 while unknown, as with a live stream.
        [[nodiscard]] double GetDuration() const;

        // Seconds.
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

        // Makes the decoder for the track once the headers are read, so set it before. The default is
        // IVideoDecoder::Create, which a factory can fall back on. Returning null leaves video out of Demux.
        void SetVideoDecoderFactory(const VideoDecoderFactory &factory) const;

        // As SetVideoDecoderFactory, with IAudioDecoder::Create as the default.
        void SetAudioDecoderFactory(const AudioDecoderFactory &factory) const;

        void SetVideoPacketCallback(const VideoPacketCallback &callback) const;

    private:
        Impl *_impl;
    };
}
