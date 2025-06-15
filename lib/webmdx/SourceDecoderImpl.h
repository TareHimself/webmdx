#pragma once
#include <optional>
#include <opus.h>
#include <vpx_codec.h>

#include "SourceReader.h"
#include "TrackPosition.h"
#include "webmdx/IAudioDecoder.h"
#include "webmdx/IVideoDecoder.h"
#include "webmdx/SourceDecoder.h"
#include "webmdx/TrackType.h"

namespace wdx {
    struct SourceDecoder::Impl {
        SourceReader reader{};
        long long byteDecodePosition = 0;
        mkvparser::Segment *segment{};
        double duration{};
        double decodedPosition{};
        long long timecodeScale = 0;
        std::vector<AudioTrack> audioTracks{};
        std::vector<VideoTrack> videoTracks{};
        std::unordered_map<int, TrackType> trackNumbersToTrackTypes{};
        std::unordered_map<int, int> trackNumbersToTrackIndexes{};
        int selectedAudioTrackIndex = 0;
        int selectedVideoTrackIndex = 0;
        const mkvparser::Cluster *cluster{};
        TrackPosition audioPosition{};
        TrackPosition videoPosition{};
        long long lastDecodedAudioPos = -1;
        long long lastDecodedVideoPos = -1;

        std::shared_ptr<IAudioDecoder> audioDecoder{};
        std::shared_ptr<IVideoDecoder> videoDecoder{};

        std::optional<VideoCallback> _videoCallback{};
        std::optional<AudioCallback> _audioCallback{};

        std::vector<uint8_t> tempBuffer{};

        void SetSource(const std::shared_ptr<ISource>& source);

        void InitVideoDecoder();

        void InitAudioDecoder();

        /**
         * Finds the best cluster after the current one for the provided timestamp
         */
        bool FindBestCluster(double timestamp,const mkvparser::Cluster* start,const mkvparser::Cluster*& best) const;

        DecodeResult Decode(double seconds);

        void Seek(double timestamp);

        void DecodeVideo(const TrackPosition &start, const TrackPosition &end, bool decodeOnly = false);

        void DecodeAudio(const TrackPosition &start, const TrackPosition &end, bool decodeOnly = false);

        TrackType GetEntryTrackType(const mkvparser::BlockEntry *entry);

        TrackType GetBlockTrackType(const mkvparser::Block *block);

        bool FindBlockOfType(const mkvparser::BlockEntry *&start, TrackType type, double time, int trackIndex);

        static const mkvparser::BlockEntry * FindRecentKeyBlock(double timestamp,const mkvparser::BlockEntry* initialEntry,const mkvparser::BlockEntry* targetBlockEntry);
        ~Impl();
    };
}
