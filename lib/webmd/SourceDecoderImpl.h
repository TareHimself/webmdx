#pragma once
#include <optional>
#include <opus.h>
#include <vpx_codec.h>

#include "SourceReader.h"
#include "TrackPosition.h"
#include "webmd/SourceDecoder.h"
#include "webmd/TrackType.h"

namespace wd {
    struct SourceDecoder::Impl {
        SourceReader reader{};
        long long byteDecodePosition = 0;
        mkvparser::Segment *segment{};
        double duration{};
        double position{};
        long long timecodeScale = 0;
        std::vector<AudioTrack> audioTracks{};
        std::vector<VideoTrack> videoTracks{};
        std::unordered_map<int, TrackType> trackNumbersToTrackTypes{};
        std::unordered_map<int, int> trackNumbersToTrackIndexes{};
        int selectedAudioTrack = 0;
        int selectedVideoTrack = 0;
        const mkvparser::Cluster *cluster{};
        TrackPosition audioPosition{};
        TrackPosition videoPosition{};
        long long lastDecodedFramePos = -1;

        vpx_codec_ctx_t videoDecoder{};
        OpusDecoder *audioDecoder{nullptr};

        std::optional<VideoCallback> _videoCallback{};
        std::optional<AudioCallback> _audioCallback{};

        void Init();

        void InitVideoDecoder();

        void InitAudioDecoder();

        bool Decode(double seconds);

        bool DecodeVideo(TrackPosition &start, TrackPosition &end);

        bool DecodeAudio(TrackPosition &start, TrackPosition &end);

        TrackType GetEntryTrackType(const mkvparser::BlockEntry *entry);

        TrackType GetBlockTrackType(const mkvparser::Block *block);

        bool FindBlockOfType(const mkvparser::BlockEntry *&start, TrackType type, double time, int trackIndex);

        ~Impl();
    };
}
