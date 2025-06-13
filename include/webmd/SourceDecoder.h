#pragma once
#include <functional>
#include <memory>
#include <optional>

#include "AudioTrack.h"
#include "ISource.h"
#include "VideoTrack.h"

namespace wd {


    class SourceDecoder {

    public:
        using FrameCallback = std::function<void(double,const std::vector<uint8_t>&)>;
        using AudioCallback = std::function<void(double,const std::vector<uint8_t>&)>;
        struct Impl;
        SourceDecoder();
        ~SourceDecoder();
        void SetSource(const std::shared_ptr<ISource>& source);
        void Decode(double seconds);
        [[nodiscard]] double GetDuration() const;
        [[nodiscard]] double GetPosition() const;
        [[nodiscard]] int GetAudioTrackCount() const;
        [[nodiscard]] int GetVideoTrackCount() const;
        [[nodiscard]] AudioTrack GetAudioTrack(int index) const;
        [[nodiscard]] VideoTrack GetVideoTrack(int index) const;
        void SetFrameCallback(const FrameCallback& callback);
        void SetAudioCallback(const AudioCallback& callback);
    private:

        Impl* _impl;
    };
}
