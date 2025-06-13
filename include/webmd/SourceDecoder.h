#pragma once
#include <functional>
#include <memory>
#include "AudioTrack.h"
#include "ISource.h"
#include "VideoTrack.h"

namespace wd {
    class SourceDecoder {
    public:
        using VideoCallback = std::function<void(double, const std::vector<uint8_t> &)>;
        using AudioCallback = std::function<void(double, const std::vector<uint8_t> &)>;
        struct Impl;

        SourceDecoder();

        ~SourceDecoder();

        void SetSource(const std::shared_ptr<ISource> &source) const;

        void Decode(double seconds) const;

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
