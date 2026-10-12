#pragma once
#include <memory>
#include <optional>
#include "SourceReader.h"
#include "TrackPosition.h"
#include "webmdx/IAudioDecoder.h"
#include "webmdx/IVideoDecoder.h"
#include "webmdx/SourceDecoder.h"
#include "webmdx/TrackType.h"

namespace wdx {
    struct SourceDecoder::Impl {
        enum class ParseState {
            NeedsEBML,
            NeedsSegment,
            NeedsHeader,
            NeedsCluster,
            Ready
        };

        // Tristate return for FindBestCluster to distinguish streaming gaps from true end-of-stream.
        enum class ClusterResult { Found, IsLast, NeedMoreData };

        SourceReader reader{};
        long long byteDecodePosition = 0;
        std::unique_ptr<mkvparser::Segment> segment{};
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
        long long lastAudioPacketPos = -1;
        long long lastVideoPacketPos = -1;
        ParseState parseState{ParseState::NeedsEBML};

        std::shared_ptr<IAudioDecoder> audioDecoder{};
        std::shared_ptr<IVideoDecoder> videoDecoder{};

        std::optional<VideoPacketCallback> _videoPacketCallback{};
        std::optional<AudioPacketCallback> _audioPacketCallback{};

        std::vector<std::uint8_t> tempBuffer{};

        void SetSource(const std::shared_ptr<ISource>& source);

        // Advances parseState toward Ready without blocking. Returns true when Ready.
        bool TryInit();

        // Extracts track info, duration, cues from already-parsed segment headers.
        void SetupFromSegmentHeaders();

        void InitVideoDecoder();

        void InitAudioDecoder();

        ClusterResult FindBestCluster(double timestamp, const mkvparser::Cluster* start, const mkvparser::Cluster*& best) const;

        DemuxResult Decode(double seconds);

        void Seek(double timestamp);

        void OutputVideoPackets(const TrackPosition &start, const TrackPosition &end);

        void OutputAudioPackets(const TrackPosition &start, const TrackPosition &end);

        void OutputVideoBlock(const mkvparser::BlockEntry *entry);

        void OutputAudioBlock(const mkvparser::BlockEntry *entry);

        // Outputs every packet a track still holds, up to timeLimit, once no later Decode will reach them.
        void OutputRemainingPackets(TrackPosition &position, TrackType type, int trackIndex, double timeLimit);

        DemuxResult Finish(bool reachedLastCluster);

        TrackType GetEntryTrackType(const mkvparser::BlockEntry *entry);

        TrackType GetBlockTrackType(const mkvparser::Block *block);

        bool FindBlockOfType(const mkvparser::BlockEntry *&start, TrackType type, double time, int trackIndex);

        static const mkvparser::BlockEntry * FindRecentKeyBlock(double timestamp, const mkvparser::BlockEntry* initialEntry, const mkvparser::BlockEntry* targetBlockEntry);

        ~Impl();

        static bool IsValidCluster(const mkvparser::Cluster* cluster);

        [[nodiscard]] IAudioDecoder * GetAudioDecoder() const;

        [[nodiscard]] IVideoDecoder * GetVideoDecoder() const;
    };
}
