#include "SourceDecoderImpl.h"
#include <cstring>
#include <iostream>
#include <span>
#include <thread>
#include "BlockEntries.h"
#include "webmdx/errors.h"
#include "webmdx/utils.h"

namespace wdx {
    void SourceDecoder::Impl::SetSource(const std::shared_ptr<ISource> &source) {
        if (segment != nullptr) {
            audioTracks = {};
            videoTracks = {};
            audioDecoder = {};
            videoDecoder = {};
            decodedPosition = 0;
            duration = 0;
            byteDecodePosition = 0;
            delete segment;
            segment = nullptr;
        }

        cluster = nullptr;
        parseState = ParseState::NeedsEBML;

        reader = SourceReader(source);
        TryInit();
    }

    bool SourceDecoder::Impl::TryInit() {
        if (parseState == ParseState::NeedsEBML) {
            long long tempPos = 0;
            mkvparser::EBMLHeader ebmlHeader;
            const auto parseResult = ebmlHeader.Parse(&reader, tempPos);
            if (parseResult != 0) {
                // E_BUFFER_NOT_FULL or other temporary/permanent parse error.
                // Return false so Decode can surface IncompleteHeader.
                return false;
            }
            byteDecodePosition = tempPos;
            parseState = ParseState::NeedsSegment;
        }

        if (parseState == ParseState::NeedsSegment) {
            if (segment) { delete segment; segment = nullptr; }
            const std::int64_t ret = mkvparser::Segment::CreateInstance(&reader, byteDecodePosition, segment);
            if (ret != 0) {
                if (segment) { delete segment; segment = nullptr; }
                return false;
            }
            parseState = ParseState::NeedsHeader;
        }

        if (parseState == ParseState::NeedsHeader) {
            const auto parseResult = segment->ParseHeaders();
            if (parseResult != 0) {
                if (parseResult == mkvparser::E_BUFFER_NOT_FULL || parseResult > 0) {
                    return false;
                }
                throw HeaderDecodeFailedException();
            }
            if (segment->GetInfo() == nullptr) {
                return false;
            }
            SetupFromSegmentHeaders();
            parseState = ParseState::NeedsCluster;
        }

        if (parseState == ParseState::NeedsCluster) {
            if (!IsValidCluster(segment->GetFirst())) {
                const auto loadResult = segment->Load();
                if (loadResult > 0 || loadResult == mkvparser::E_BUFFER_NOT_FULL) {
                    return false;
                }
                if (loadResult < 0 && loadResult != mkvparser::E_PARSE_FAILED) {
                    if (!IsValidCluster(segment->GetFirst())) {
                        throw SourceDecodeException();
                    }
                }
            }
            if (!IsValidCluster(segment->GetFirst())) {
                return false;
            }

            // Resolve duration when it wasn't encoded in the segment headers.
            if (duration < 0) {
                if (reader.source->GetLength() >= 0) {
                    const auto* last = segment->GetLast();
                    duration = (last && !last->EOS())
                        ? nanoSecsToSecs(last->GetTime())
                        : 0.0;
                } else {
                    duration = 0.0;  // streaming with unknown total length
                }
            }

            cluster = segment->GetFirst();

            if (!audioTracks.empty()) {
                InitAudioDecoder();
            }
            if (!videoTracks.empty()) {
                InitVideoDecoder();
            }

            parseState = ParseState::Ready;
        }

        return true;
    }

    void SourceDecoder::Impl::SetupFromSegmentHeaders() {
        const auto segmentInfo = segment->GetInfo();

        // duration == -1 sentinel means "not yet resolved" (needs clusters to be loaded first).
        const auto rawDuration = segmentInfo->GetDuration();
        duration = (rawDuration != -1) ? nanoSecsToSecs(rawDuration) : -1.0;

        timecodeScale = segmentInfo->GetTimeCodeScale();

        const auto tracks = segment->GetTracks();
        const auto numTracks = tracks->GetTracksCount();

        const auto cues = segment->GetCues();
        if (cues != nullptr) {
            while (!cues->DoneParsing()) {
                if (!cues->LoadCuePoint()) {
                    break;
                }
            }
        }

        for (auto i = 0; i < numTracks; i++) {
            switch (const auto track = tracks->GetTrackByIndex(i); track->GetType()) {
                case mkvparser::Track::kAudio: {
                    const auto trackNumber = track->GetNumber();
                    const auto asAudio = dynamic_cast<const mkvparser::AudioTrack *>(track);
                    AudioTrack audioTrack{};
                    audioTrack.channels = static_cast<int>(asAudio->GetChannels());
                    audioTrack.sampleRate = static_cast<int>(asAudio->GetSamplingRate());
                    audioTrack.bitDepth = static_cast<int>(asAudio->GetBitDepth());
                    audioTrack.codecDelay = static_cast<double>(asAudio->GetCodecDelay());
                    audioTrack.seekPreRoll = nanoSecsToSecs(static_cast<long long>(asAudio->GetSeekPreRoll()));
                    size_t codecPrivateSize = 0;
                    audioTrack.codecPrivate = asAudio->GetCodecPrivate(codecPrivateSize);
                    audioTrack.codecPrivateSize = codecPrivateSize;

                    auto codec = asAudio->GetCodecId();
                    if (strcmp(codec, "A_OPUS") == 0) {
                        audioTrack.codec = AudioCodec::Opus;
                    } else if (strcmp(codec, "A_VORBIS") == 0) {
                        audioTrack.codec = AudioCodec::Vorbis;
                    }

                    trackNumbersToTrackIndexes.emplace(trackNumber, audioTracks.size());
                    audioTracks.push_back(audioTrack);
                    trackNumbersToTrackTypes.emplace(trackNumber, TrackType::Audio);
                }
                break;
                case mkvparser::Track::kVideo: {
                    const auto trackNumber = track->GetNumber();
                    const auto asVideo = dynamic_cast<const mkvparser::VideoTrack *>(track);

                    VideoTrack videoTrack{};
                    videoTrack.width = static_cast<int>(asVideo->GetWidth());
                    videoTrack.height = static_cast<int>(asVideo->GetHeight());
                    auto codec = asVideo->GetCodecId();
                    if (strcmp(codec, "V_VP9") == 0) {
                        videoTrack.codec = VideoCodec::Vpx9;
                    } else if (strcmp(codec, "V_VP8") == 0) {
                        videoTrack.codec = VideoCodec::Vpx8;
                    } else if (strcmp(codec, "V_AV1") == 0) {
                        videoTrack.codec = VideoCodec::Av1;
                    }

                    trackNumbersToTrackIndexes.emplace(trackNumber, videoTracks.size());
                    videoTracks.push_back(videoTrack);
                    trackNumbersToTrackTypes.emplace(trackNumber, TrackType::Video);
                }
                break;
                default:
                    break;
            }
        }
    }

    void SourceDecoder::Impl::InitVideoDecoder() {
        lastAudioPacketPos = -1;
        videoPosition = {};
        videoPosition.cluster = cluster;
        cluster->GetFirst(videoPosition.entry);
        FindBlockOfType(videoPosition.entry, TrackType::Video, decodedPosition, selectedVideoTrackIndex);
        const auto &track = videoTracks[selectedVideoTrackIndex];
        videoDecoder = IVideoDecoder::Create(track);
    }

    void SourceDecoder::Impl::InitAudioDecoder() {
        lastVideoPacketPos = -1;
        audioPosition = {};
        audioPosition.cluster = cluster;
        cluster->GetFirst(audioPosition.entry);
        FindBlockOfType(audioPosition.entry, TrackType::Audio, decodedPosition, selectedAudioTrackIndex);
        const auto &track = audioTracks[selectedAudioTrackIndex];
        audioDecoder = IAudioDecoder::Create(track);
    }

    SourceDecoder::Impl::ClusterResult SourceDecoder::Impl::FindBestCluster(
        double timestamp, const mkvparser::Cluster* start, const mkvparser::Cluster*& best) const
    {
        auto targetCluster = start;
        do {
            if (timestamp <= nanoSecsToSecs(targetCluster->GetLastTime())) {
                best = targetCluster;
                return ClusterResult::Found;
            }

            auto next = segment->GetNext(targetCluster);

            if (next == nullptr || next->EOS()) {
                // Try to load more clusters from the source.
                const auto loadResult = segment->Load();
                if (loadResult > 0 || loadResult == mkvparser::E_BUFFER_NOT_FULL) {
                    best = targetCluster;
                    return ClusterResult::NeedMoreData;
                }
                // loadResult == 0 (all loaded) or E_PARSE_FAILED — try GetNext once more.
                next = segment->GetNext(targetCluster);
                if (next == nullptr || next->EOS()) {
                    best = targetCluster;
                    return ClusterResult::IsLast;
                }
            }

            targetCluster = next;
        } while (true);
    }

    DemuxResult SourceDecoder::Impl::Decode(const double seconds) {
        if (parseState != ParseState::Ready) {
            if (!TryInit()) {
                return parseState <= ParseState::NeedsHeader
                    ? DemuxResult::IncompleteHeader
                    : DemuxResult::IncompleteCluster;
            }
        }

        if (duration > 0 && decodedPosition >= duration) {
            return DemuxResult::Finished;
        }

        const auto hasAudio = audioDecoder && !audioTracks.empty();
        const auto hasVideo = videoDecoder && !videoTracks.empty();

        if (!hasAudio && !hasVideo) {
            throw NoTracksAvailableException();
        }

        // Compute target position but don't commit it until we confirm data is available.
        const auto targetPosition = (duration > 0)
            ? std::min(decodedPosition + seconds, duration)
            : decodedPosition + seconds;

        const mkvparser::Cluster* bestCluster = cluster;
        const auto clusterResult = FindBestCluster(targetPosition, cluster, bestCluster);

        if (clusterResult == ClusterResult::NeedMoreData) {
            return DemuxResult::IncompleteCluster;
        }

        const bool isLastCluster = (clusterResult == ClusterResult::IsLast);
        const auto initialCluster = cluster;
        cluster = bestCluster;

        // Early exit: we know we're past the end already.
        if (isLastCluster && duration > 0 && targetPosition >= duration) {
            decodedPosition = targetPosition;
            return DemuxResult::Finished;
        }

        decltype(TrackPosition::entry) initialAudio = audioPosition.entry;
        decltype(TrackPosition::entry) initialVideo = videoPosition.entry;
        decltype(TrackPosition::entry) finalAudio = initialAudio;
        decltype(TrackPosition::entry) finalVideo = initialVideo;

        if (initialCluster != cluster) {
            cluster->GetFirst(finalAudio);
            cluster->GetFirst(finalVideo);
        }

        if (hasAudio) {
            FindBlockOfType(finalAudio, TrackType::Audio, targetPosition, selectedAudioTrackIndex);
        }
        if (hasVideo) {
            FindBlockOfType(finalVideo, TrackType::Video, targetPosition, selectedVideoTrackIndex);
        }

        if (hasAudio && initialAudio != finalAudio) {
            const auto initial = audioPosition;
            audioPosition.cluster = cluster;
            audioPosition.entry = finalAudio;
            audioPosition.UseBlockTime();
            OutputAudioPackets(initial, audioPosition);
        }

        if (hasVideo && initialVideo != finalVideo) {
            const auto initial = videoPosition;
            videoPosition.cluster = cluster;
            videoPosition.entry = finalVideo;
            videoPosition.UseBlockTime();
            OutputVideoPackets(initial, videoPosition);
        }

        decodedPosition = targetPosition;

        // IsLast means no more clusters exist (truly end of stream for streaming sources,
        // or end of file for file sources).
        if (isLastCluster && (duration <= 0 || decodedPosition >= duration)) {
            return DemuxResult::Finished;
        }

        if (duration > 0 && decodedPosition >= duration) {
            return DemuxResult::Finished;
        }

        return DemuxResult::Success;
    }

    void SourceDecoder::Impl::Seek(double timestamp) {
        decodedPosition = timestamp;
        const mkvparser::Cluster* seekCluster = nullptr;
        FindBestCluster(timestamp, segment->GetFirst(), seekCluster);
        cluster = seekCluster ? seekCluster : segment->GetFirst();

        if (!audioTracks.empty()) {

            audioDecoder->Reset();
            const auto &track = audioTracks[selectedAudioTrackIndex];

            // TODO: Look into the right way to do this
            if (track.seekPreRoll > 0) {
                const mkvparser::Cluster* preRollCluster{};

                auto preRollTime = timestamp - track.seekPreRoll;
                FindBestCluster(preRollTime, segment->GetFirst(), preRollCluster);
                audioPosition.SetCluster(preRollCluster);
                FindBlockOfType(audioPosition.entry, TrackType::Audio, preRollTime, selectedAudioTrackIndex);
                audioPosition.UseBlockTime();

                const auto initial = audioPosition;

                audioPosition.SetCluster(cluster);
                FindBlockOfType(audioPosition.entry, TrackType::Audio, timestamp, selectedAudioTrackIndex);
                audioPosition.UseBlockTime();
                OutputAudioPackets(initial, audioPosition);
            }
            else {
                audioPosition.SetCluster(cluster);
                FindBlockOfType(audioPosition.entry, TrackType::Audio, timestamp, selectedAudioTrackIndex);
                audioPosition.UseBlockTime();
            }
        }

        if (!videoTracks.empty()) {
            videoDecoder->Reset();

            videoPosition.SetCluster(cluster);
            FindBlockOfType(videoPosition.entry, TrackType::Video, timestamp, selectedVideoTrackIndex);
            videoPosition.UseBlockTime();

            const mkvparser::BlockEntry* trackEntry;
            segment->GetFirst()->GetFirst(trackEntry);
            FindBlockOfType(trackEntry, TrackType::Video, 0, selectedVideoTrackIndex);
            trackEntry = FindRecentKeyBlock(timestamp, trackEntry, videoPosition.entry);
            if (trackEntry != nullptr) {
                TrackPosition initial{};
                initial.cluster = trackEntry->GetCluster();
                initial.entry = trackEntry;
                initial.UseBlockTime();

                OutputVideoPackets(initial, videoPosition);
            }
        }
    }

    void SourceDecoder::Impl::OutputVideoPackets(const TrackPosition &start, const TrackPosition &end) {
        BlockEntries entries{start, end};
        for (const auto entry : entries) {
            const auto block = entry->GetBlock();
            const auto frameCount = block->GetFrameCount();
            const auto time = nanoSecsToSecs(block->GetTime(entry->GetCluster()));
            const auto isKeyBlock = block->IsKey();
            for (auto i = 0; i < frameCount; i++) {
                auto frame = block->GetFrame(i);

                if (frame.pos == lastVideoPacketPos) {
                    continue;
                }
                if (tempBuffer.size() < frame.len) {
                    tempBuffer.resize(frame.len);
                }
                frame.Read(&reader, tempBuffer.data());
                if (_videoPacketCallback.has_value())
                {
                    auto packet = std::make_shared<Packet>(time, std::span(tempBuffer.data(), frame.len), isKeyBlock && i == 0);
                    (*_videoPacketCallback)(packet, videoDecoder.get());
                }

                lastVideoPacketPos = frame.pos;
            }
        }
    }

    void SourceDecoder::Impl::OutputAudioPackets(const TrackPosition &start, const TrackPosition &end) {
        const BlockEntries entries{start, end};

        std::vector<float> samples{};
        for (const auto entry : entries) {
            const auto block = entry->GetBlock();
            const auto time = nanoSecsToSecs(block->GetTime(entry->GetCluster()));
            const auto frameCount = block->GetFrameCount();
            const auto isKeyBlock = block->IsKey();
            for (auto i = 0; i < frameCount; i++) {
                const auto frame = block->GetFrame(i);
                if (frame.pos == lastAudioPacketPos) {
                    continue;
                }
                if (tempBuffer.size() < frame.len) {
                    tempBuffer.resize(frame.len);
                }
                frame.Read(&reader, tempBuffer.data());
                if (_audioPacketCallback.has_value())
                {
                    auto packet = std::make_shared<Packet>(time, std::span(tempBuffer.data(), frame.len), isKeyBlock && i == 0);
                    (*_audioPacketCallback)(packet, audioDecoder.get());
                }

                lastAudioPacketPos = frame.pos;
            }
        }
    }

    TrackType SourceDecoder::Impl::GetEntryTrackType(const mkvparser::BlockEntry *entry) {
        return GetBlockTrackType(entry->GetBlock());
    }

    TrackType SourceDecoder::Impl::GetBlockTrackType(const mkvparser::Block *block) {
        const auto track = static_cast<int>(block->GetTrackNumber());
        return trackNumbersToTrackTypes[track];
    }

    bool SourceDecoder::Impl::FindBlockOfType(const mkvparser::BlockEntry *&start, TrackType type, double time,
                                              int trackIndex) {
        const mkvparser::BlockEntry *initial = start;
        while (!start->EOS()) {
            auto track = static_cast<int>(start->GetBlock()->GetTrackNumber());
            auto trackType = trackNumbersToTrackTypes[track];

            const mkvparser::BlockEntry *next;
            start->GetCluster()->GetNext(start, next);

            if (trackType != type) {
                start = next;
                initial = next;
                continue;
            }

            // the track index in the specific track vector
            auto localTrackIndex = trackNumbersToTrackIndexes[track];

            if (trackIndex != localTrackIndex) {
                start = next;
                initial = next;
                continue;
            }

            auto startTime = nanoSecsToSecs(start->GetBlock()->GetTime(start->GetCluster()));

            if (startTime > time) {
                break;
            }

            if (startTime == time) {
                return true;
            }

            while (next != nullptr && !next->EOS() && next->GetBlock()->GetTrackNumber() != track) {
                next->GetCluster()->GetNext(next, next);
            }

            // If next is null we are as close as possible within this cluster
            if (next == nullptr || next->EOS()) {
                return true;
            }

            const auto nextTime = nanoSecsToSecs(next->GetBlock()->GetTime(next->GetCluster()));

            if (time >= nextTime) {
                start = next;
                continue;
            }

            if (time < nextTime) {
                return true;
            }

            break;
        }
        start = initial;
        return false;
    }

    const mkvparser::BlockEntry * SourceDecoder::Impl::FindRecentKeyBlock(double timestamp, const mkvparser::BlockEntry* initialEntry, const mkvparser::BlockEntry* targetBlockEntry) {
        // Should optimize using cue's if they are available
        if (targetBlockEntry->GetBlock()->IsKey()) {
            return targetBlockEntry;
        }
        TrackPosition begin{};
        TrackPosition end{};
        begin.cluster = initialEntry->GetCluster();
        begin.entry = initialEntry;
        begin.UseBlockTime();
        end.cluster = targetBlockEntry->GetCluster();
        end.entry = targetBlockEntry;
        end.UseBlockTime();

        const mkvparser::BlockEntry* result = nullptr;
        BlockEntries entries{begin, end};
        for (const auto entry : entries) {
            const auto block = entry->GetBlock();
            const auto blockTime = nanoSecsToSecs(block->GetTime(entry->GetCluster()));
            if (blockTime > timestamp) {
                break;
            }

            if (block->IsKey()) {
                result = entry;
            }
        }

        return result;
    }

    SourceDecoder::Impl::~Impl() {
        delete segment;
        segment = nullptr;
    }

    bool SourceDecoder::Impl::IsValidCluster(const mkvparser::Cluster* cluster)
    {
        return cluster != nullptr && !cluster->EOS();
    }

    IAudioDecoder* SourceDecoder::Impl::GetAudioDecoder() const
    {
        return audioDecoder.get();
    }

    IVideoDecoder* SourceDecoder::Impl::GetVideoDecoder() const
    {
        return videoDecoder.get();
    }
}
