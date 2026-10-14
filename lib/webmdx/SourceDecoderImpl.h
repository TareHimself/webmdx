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
        // Bytes the source has to hold for the step that ran out to go on, 0 when none did.
        std::int64_t bytesNeeded = 0;
        // Where the missing bytes start, bytesNeeded is where they end.
        std::int64_t neededStart = 0;
        // What the source was last told it can drop.
        std::int64_t released = 0;
        // Set once the file is known to have no usable index.
        bool hasNoCues = false;
        // Headers reaching past this are rejected, 0 for no limit.
        std::int64_t maxHeaderSize = 4 * 1024 * 1024;

        std::shared_ptr<IAudioDecoder> audioDecoder{};
        std::shared_ptr<IVideoDecoder> videoDecoder{};
        AudioDecoderFactory audioDecoderFactory{IAudioDecoder::Create};
        VideoDecoderFactory videoDecoderFactory{IVideoDecoder::Create};

        std::optional<VideoPacketCallback> _videoPacketCallback{};
        std::optional<AudioPacketCallback> _audioPacketCallback{};

        std::vector<std::uint8_t> tempBuffer{};

        // Reads nothing, Prepare does.
        void SetSource(const std::shared_ptr<ISource>& source);

        // Advances parseState toward Ready without blocking. Returns true when Ready.
        bool TryInit();

        // Extracts track info, duration, cues from already-parsed segment headers.
        void SetupFromSegmentHeaders();

        void InitVideoDecoder();

        void InitAudioDecoder();

        ClusterResult FindBestCluster(double timestamp, const mkvparser::Cluster* start, const mkvparser::Cluster*& best);

        // Sets bytesNeeded to end, or to any amount more when end is 0 or less.
        void Need(long long end);

        // Asks the source to reach end. False, with bytesNeeded set, when it could not.
        bool Request(long long end);

        // How much of a cluster the source holds so far.
        struct ClusterExtent {
            bool complete{};   // every block was parsed, later blocks can no longer arrive
            bool hasBlocks{};  // at least one block was parsed
            double lastTime{}; // time of the last parsed block
            long long needed{}; // where the source has to reach for the next block, when not complete
        };

        // Parses what the source holds of a cluster, without pulling.
        ClusterExtent MeasureCluster(const mkvparser::Cluster* target);

        // Bytes held in an unbroken run from where mkvparser is working.
        [[nodiscard]] std::int64_t Held() const;

        // Loads the index when the file has one that is not in its headers. False when there is none to use for now.
        bool LoadCues();

        // The cluster the index gives for time, null when the seek has to walk from the first cluster instead.
        // bytesNeeded is set when that cluster is known but not held.
        const mkvparser::Cluster* FindCueCluster(double time);

        // The cluster after current, loaded if need be.
        ClusterResult NextCluster(const mkvparser::Cluster* current, const mkvparser::Cluster*& next);

        // True for a real parse error, false for running out of data. Clear reader.ranShort before the mkvparser call.
        [[nodiscard]] bool IsRejected(long long status) const;

        // No more data will arrive.
        [[nodiscard]] bool SourceIsComplete() const;

        // libwebm lists a block before its payload has arrived.
        [[nodiscard]] bool IsBlockAvailable(const mkvparser::BlockEntry* entry) const;

        // Finds the first block of a track that has had none, up to the current cluster.
        bool LocateTrack(TrackPosition& position, TrackType type, int trackIndex);

        // Moves entry to the track's block covering time, if it has one.
        const mkvparser::BlockEntry* AdvanceTrack(const mkvparser::BlockEntry* entry, TrackType type, double time, int trackIndex);

        // Success once ready, otherwise what Demux and Seek should return.
        DemuxResult Prepare();

        DemuxResult Decode(double seconds);

        // Changes nothing unless it returns Success.
        DemuxResult Seek(double timestamp);

        // Tells the source what the decoder is done with.
        void ReleaseConsumed();

        void OutputVideoPackets(const TrackPosition &start, const TrackPosition &end);

        void OutputAudioPackets(const TrackPosition &start, const TrackPosition &end);

        void OutputVideoBlock(const mkvparser::BlockEntry *entry);

        void OutputAudioBlock(const mkvparser::BlockEntry *entry);

        // Outputs every packet a track still holds, up to timeLimit, once no later Decode will reach them.
        void OutputRemainingPackets(TrackPosition &position, TrackType type, int trackIndex, double timeLimit);

        DemuxResult Finish(bool reachedLastCluster);

        // False for other tracks, including ones that are neither audio nor video.
        [[nodiscard]] bool IsBlockOfTrack(const mkvparser::Block *block, TrackType type, int trackIndex) const;

        bool FindBlockOfType(const mkvparser::BlockEntry *&start, TrackType type, double time, int trackIndex);

        static const mkvparser::BlockEntry * FindRecentKeyBlock(double timestamp, const mkvparser::BlockEntry* initialEntry, const mkvparser::BlockEntry* targetBlockEntry);

        ~Impl();

        static bool IsValidCluster(const mkvparser::Cluster* cluster);

        [[nodiscard]] IAudioDecoder * GetAudioDecoder() const;

        [[nodiscard]] IVideoDecoder * GetVideoDecoder() const;
    };
}
