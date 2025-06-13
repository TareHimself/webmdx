#pragma once
#include <functional>
#include <memory>
#include <span>

#include "AudioTrack.h"
#include "DecodeResult.h"
#include "ISource.h"
#include "VideoTrack.h"

namespace wd {
    class SourceDecoder {
    public:
        using VideoCallback = std::function<void(double, const std::span<uint8_t> &)>;
        using AudioCallback = std::function<void(double, const std::span<float> &)>;
        struct Impl;

        SourceDecoder();

        ~SourceDecoder();

        void SetSource(const std::shared_ptr<ISource> &source) const;

        [[nodiscard]] DecodeResult Decode(double seconds) const;

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

        void SetVideoCallback(const VideoCallback &callback);

        void SetAudioCallback(const AudioCallback &callback);

    private:
        Impl *_impl;
    };
}
