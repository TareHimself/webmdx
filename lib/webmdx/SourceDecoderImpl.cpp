#include "SourceDecoderImpl.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <thread>
#include <webm/common/webmids.h>
#include "BlockEntries.h"
#include "webmdx/errors.h"
#include "webmdx/utils.h"

namespace wdx {
    void SourceDecoder::Impl::SetSource(const std::shared_ptr<ISource> &source) {
        if (segment != nullptr) {
            audioTracks = {};
            videoTracks = {};
            trackNumbersToTrackTypes = {};
            trackNumbersToTrackIndexes = {};
            audioDecoder = {};
            videoDecoder = {};
            decodedPosition = 0;
            duration = 0;
            byteDecodePosition = 0;
            segment.reset();
        }

        cluster = nullptr;
        released = 0;
        parseState = ParseState::NeedsEBML;

        bytesNeeded = 0;
        reader = SourceReader(source);
    }

    bool SourceDecoder::Impl::TryInit() {
        reader.focus = 0;
        // Any amount more, unless mkvparser gives an offset below.
        Need(0);

        if (parseState == ParseState::NeedsEBML) {
            long long tempPos = 0;
            mkvparser::EBMLHeader ebmlHeader;
            reader.ranShort = false;
            const auto parseResult = ebmlHeader.Parse(&reader, tempPos);
            if (parseResult != 0) {
                if (IsRejected(parseResult)) {
                    throw HeaderDecodeFailedException();
                }
                Need(parseResult);
                return false;
            }
            byteDecodePosition = tempPos;
            parseState = ParseState::NeedsSegment;
        }

        if (parseState == ParseState::NeedsSegment) {
            segment.reset();
            mkvparser::Segment* rawSegment = nullptr;
            reader.ranShort = false;
            const std::int64_t ret = mkvparser::Segment::CreateInstance(&reader, byteDecodePosition, rawSegment);
            segment.reset(rawSegment);
            if (ret != 0) {
                segment.reset();
                if (IsRejected(ret)) {
                    throw HeaderDecodeFailedException();
                }
                Need(ret);
                return false;
            }
            parseState = ParseState::NeedsHeader;
        }

        if (parseState == ParseState::NeedsHeader) {
            reader.ranShort = false;
            const auto parseResult = segment->ParseHeaders();
            if (parseResult != 0) {
                if (IsRejected(parseResult)) {
                    throw HeaderDecodeFailedException();
                }
                Need(parseResult);
                return false;
            }
            if (segment->GetInfo() == nullptr) {
                return false;
            }
            SetupFromSegmentHeaders();
            parseState = ParseState::NeedsCluster;
        }

        if (parseState == ParseState::NeedsCluster) {
            // Segment::Load only works once, LoadCluster resumes.
            long long neededAt = 0;
            long neededLength = 0;
            if (!IsValidCluster(segment->GetFirst())) {
                reader.ranShort = false;
                const auto loadResult = segment->LoadCluster(neededAt, neededLength);
                if (IsRejected(loadResult)) {
                    throw SourceDecodeException();
                }
                if (loadResult != 0) {
                    Need(loadResult == mkvparser::E_BUFFER_NOT_FULL ? neededAt + neededLength : 0);
                    return false;
                }
            }

            // The cluster's header can arrive before its first block.
            const auto firstCluster = segment->GetFirst();
            if (firstCluster->GetEntryCount() <= 0) {
                reader.ranShort = false;
                const auto firstResult = firstCluster->Parse(neededAt, neededLength);
                if (IsRejected(firstResult)) {
                    throw SourceDecodeException();
                }
                if (firstCluster->GetEntryCount() <= 0) {
                    Need(firstResult == mkvparser::E_BUFFER_NOT_FULL ? neededAt + neededLength : 0);
                    return false;
                }
            }

            // Resolve duration when it wasn't encoded in the segment headers.
            if (duration < 0) {
                duration = 0.0;  // unknown while the source is still arriving
                if (SourceIsComplete()) {
                    const mkvparser::Cluster* last = segment->GetFirst();
                    const mkvparser::Cluster* next = nullptr;
                    while (NextCluster(last, next) == ClusterResult::Found) {
                        last = next;
                    }
                    duration = nanoSecsToSecs(last->GetTime());
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
        if (!videoDecoder) {
            videoDecoder = videoDecoderFactory(track);
        }
    }

    void SourceDecoder::Impl::InitAudioDecoder() {
        lastVideoPacketPos = -1;
        audioPosition = {};
        audioPosition.cluster = cluster;
        cluster->GetFirst(audioPosition.entry);
        FindBlockOfType(audioPosition.entry, TrackType::Audio, decodedPosition, selectedAudioTrackIndex);
        const auto &track = audioTracks[selectedAudioTrackIndex];
        if (!audioDecoder) {
            audioDecoder = audioDecoderFactory(track);
        }
    }

    SourceDecoder::Impl::ClusterResult SourceDecoder::Impl::FindBestCluster(
        double timestamp, const mkvparser::Cluster* start, const mkvparser::Cluster*& best)
    {
        auto targetCluster = start;
        best = start;
        do {
            const auto extent = MeasureCluster(targetCluster);
            if (extent.hasBlocks) {
                best = targetCluster;
                if (timestamp <= extent.lastTime) {
                    return ClusterResult::Found;
                }
            }

            // Later clusters can't be reached until this one is whole.
            if (!extent.complete) {
                if (Request(extent.needed)) {
                    continue;
                }
                return ClusterResult::NeedMoreData;
            }

            const mkvparser::Cluster* next = nullptr;
            if (const auto result = NextCluster(targetCluster, next); result != ClusterResult::Found) {
                return result;
            }

            targetCluster = next;
        } while (true);
    }

    SourceDecoder::Impl::ClusterExtent SourceDecoder::Impl::MeasureCluster(const mkvparser::Cluster* target) {
        ClusterExtent extent{};
        reader.focus = target->m_element_start;
        long long neededAt = 0;
        long neededLength = 0;
        long status = 0;
        reader.ranShort = false;
        while (status == 0) {
            status = target->Parse(neededAt, neededLength);
        }

        if (status > 0) {
            extent.complete = true;
        } else {
            // A source that ended mid-cluster ends at its last whole block.
            extent.complete = SourceIsComplete();
            if (!extent.complete && IsRejected(status)) {
                throw SourceDecodeException();
            }
            extent.needed = status == mkvparser::E_BUFFER_NOT_FULL ? neededAt + neededLength : 0;
        }

        const mkvparser::BlockEntry* last = nullptr;
        if (const auto count = target->GetEntryCount(); count > 0) {
            target->GetEntry(count - 1, last);
        }

        // Listed blocks can still be missing their data: the last one while the source grows, any of them once
        // the source has dropped what it was told it could.
        const auto held = Held();
        if (last != nullptr && BlockEnd(last) > held) {
            extent.complete = SourceIsComplete();
            extent.needed = std::max(extent.needed, BlockEnd(last));
        }
        while (last != nullptr && BlockEnd(last) > held) {
            const auto index = last->GetIndex();
            last = nullptr;
            if (index > 0) {
                target->GetEntry(index - 1, last);
            }
        }

        if (last != nullptr) {
            extent.hasBlocks = true;
            extent.lastTime = nanoSecsToSecs(last->GetBlock()->GetTime(target));
        }
        return extent;
    }

    SourceDecoder::Impl::ClusterResult SourceDecoder::Impl::NextCluster(const mkvparser::Cluster* current, const mkvparser::Cluster*& next) {
        for (;;) {
            reader.focus = current->m_element_start;
            reader.ranShort = false;
            long long neededAt = 0;
            long neededLength = 0;
            // Unlike LoadCluster this also carries on from a cluster that a seek jumped to.
            const auto status = segment->ParseNext(current, next, neededAt, neededLength);
            if (status == 0 && IsValidCluster(next)) {
                return ClusterResult::Found;
            }
            if (status >= 0 || SourceIsComplete()) {
                return ClusterResult::IsLast;
            }
            if (IsRejected(status)) {
                throw SourceDecodeException();
            }
            if (!Request(status == mkvparser::E_BUFFER_NOT_FULL ? neededAt + neededLength : 0)) {
                return ClusterResult::NeedMoreData;
            }
        }
    }

    bool SourceDecoder::Impl::IsRejected(const long long status) const {
        return status < 0 && status != mkvparser::E_BUFFER_NOT_FULL && !reader.ranShort;
    }

    std::int64_t SourceDecoder::Impl::Held() const {
        return reader.source->GetAvailableFrom(reader.focus);
    }

    void SourceDecoder::Impl::Need(const long long end) {
        neededStart = Held();
        bytesNeeded = std::max<std::int64_t>(end, neededStart + 1);
    }

    bool SourceDecoder::Impl::Request(const long long end) {
        Need(end);
        reader.source->MakeRangeAvailable(neededStart, static_cast<std::uint64_t>(bytesNeeded - neededStart));
        if (Held() < bytesNeeded) {
            return false;
        }
        bytesNeeded = 0;
        return true;
    }

    bool SourceDecoder::Impl::SourceIsComplete() const {
        const auto length = reader.source->GetLength();
        return length >= 0 && Held() >= length;
    }

    bool SourceDecoder::Impl::IsBlockAvailable(const mkvparser::BlockEntry* entry) const {
        return BlockEnd(entry) <= reader.source->GetAvailableFrom(entry->GetBlock()->m_start);
    }

    std::int64_t SourceDecoder::Impl::BlockEnd(const mkvparser::BlockEntry* entry) {
        const auto block = entry->GetBlock();
        return block->m_start + block->m_size;
    }

    bool SourceDecoder::Impl::LoadCues() {
        if (segment->GetCues() == nullptr && !hasNoCues) {
            // Many files keep the index after the last cluster, and say where in their headers.
            long long offset = -1;
            if (const auto seekHead = segment->GetSeekHead()) {
                for (auto i = 0; i < seekHead->GetCount(); i++) {
                    if (const auto entry = seekHead->GetEntry(i); entry != nullptr && entry->id == libwebm::kMkvCues) {
                        offset = entry->pos;
                    }
                }
            }
            long status = offset >= 0 ? mkvparser::E_BUFFER_NOT_FULL : 1;
            reader.focus = segment->m_start + offset;
            while (status == mkvparser::E_BUFFER_NOT_FULL) {
                long long neededAt = 0;
                long neededLength = 0;
                status = segment->ParseCues(offset, neededAt, neededLength);
                if (status == mkvparser::E_BUFFER_NOT_FULL && !Request(neededAt + neededLength)) {
                    // The walk needs less than the far end of the file, so this is not worth asking the consumer for.
                    bytesNeeded = 0;
                    return false;
                }
            }
            hasNoCues = status != 0;
        }

        const auto cues = segment->GetCues();
        if (cues == nullptr) {
            return false;
        }
        // The walk past an index notes where it is without needing it to be there.
        reader.focus = cues->m_element_start;
        if (const auto end = cues->m_element_start + cues->m_element_size; Held() < end && !Request(end)) {
            bytesNeeded = 0;
            return false;
        }
        while (!cues->DoneParsing()) {
            if (!cues->LoadCuePoint()) {
                break;
            }
        }
        return cues->GetCount() > 0;
    }

    const mkvparser::Cluster* SourceDecoder::Impl::FindCueCluster(const double time) {
        bytesNeeded = 0;
        if (time <= 0 || !LoadCues()) {
            return nullptr;
        }

        // The index is usually only kept for video.
        const auto type = videoTracks.empty() ? TrackType::Audio : TrackType::Video;
        const auto trackIndex = videoTracks.empty() ? selectedAudioTrackIndex : selectedVideoTrackIndex;
        const mkvparser::Track* track = nullptr;
        for (const auto& [number, numberType] : trackNumbersToTrackTypes) {
            if (numberType == type && trackNumbersToTrackIndexes.at(number) == trackIndex) {
                track = segment->GetTracks()->GetTrackByNumber(number);
            }
        }

        const auto timeNs = static_cast<long long>(time * 1000000000.0);
        const mkvparser::CuePoint* point = nullptr;
        const mkvparser::CuePoint::TrackPosition* place = nullptr;
        if (!segment->GetCues()->Find(timeNs, track, point, place) || point->GetTime(segment.get()) > timeNs) {
            return nullptr;
        }

        const auto found = segment->FindOrPreloadCluster(place->m_pos);
        if (found == nullptr) {
            return nullptr;
        }
        reader.focus = found->m_element_start;
        for (;;) {
            long long neededAt = 0;
            long neededLength = 0;
            const auto status = found->Load(neededAt, neededLength);
            if (status == 0) {
                return found;
            }
            if (status != mkvparser::E_BUFFER_NOT_FULL) {
                bytesNeeded = 0;
                return nullptr;
            }
            if (!Request(neededAt + neededLength)) {
                return nullptr;
            }
        }
    }

    bool SourceDecoder::Impl::LocateTrack(TrackPosition& position, const TrackType type, const int trackIndex) {
        // position.cluster is the first cluster not yet known to hold no block of the track
        for (;;) {
            const mkvparser::BlockEntry* entry = nullptr;
            position.cluster->GetFirst(entry);
            while (entry != nullptr && !entry->EOS()) {
                if (IsBlockOfTrack(entry->GetBlock(), type, trackIndex)) {
                    position.entry = entry;
                    position.UseBlockTime();
                    return true;
                }
                position.cluster->GetNext(entry, entry);
            }

            // More of the current cluster may still arrive, the ones before it are done.
            if (position.cluster == cluster) {
                return false;
            }
            const auto next = NextKnownCluster(position.cluster);
            if (!IsValidCluster(next)) {
                return false;
            }
            position.cluster = next;
        }
    }

    const mkvparser::BlockEntry* SourceDecoder::Impl::AdvanceTrack(const mkvparser::BlockEntry* entry, const TrackType type, const double time, const int trackIndex) {
        auto found = entry;
        if (found->GetCluster() != cluster) {
            cluster->GetFirst(found);
        }
        FindBlockOfType(found, type, time, trackIndex);
        // No block of this track in the cluster yet.
        return found != nullptr ? found : entry;
    }

    DemuxResult SourceDecoder::Impl::Prepare() {
        bytesNeeded = 0;
        for (;;) {
            if (parseState == ParseState::Ready || TryInit()) {
                bytesNeeded = 0;
                return DemuxResult::Success;
            }
            // Probably not WebM.
            if (parseState <= ParseState::NeedsHeader && maxHeaderSize > 0 && bytesNeeded > maxHeaderSize) {
                throw HeaderDecodeFailedException();
            }
            if (!Request(bytesNeeded)) {
                break;
            }
        }
        const auto inHeader = parseState <= ParseState::NeedsHeader;
        // Nothing more is coming.
        if (SourceIsComplete()) {
            if (inHeader) {
                throw HeaderDecodeFailedException();
            }
            return DemuxResult::Finished;
        }
        return inHeader ? DemuxResult::IncompleteHeader : DemuxResult::IncompleteCluster;
    }

    DemuxResult SourceDecoder::Impl::Decode(const double seconds) {
        if (const auto prepared = Prepare(); prepared != DemuxResult::Success) {
            return prepared;
        }

        const auto hasAudio = audioDecoder && !audioTracks.empty();
        const auto hasVideo = videoDecoder && !videoTracks.empty();

        if (!hasAudio && !hasVideo) {
            throw NoTracksAvailableException();
        }

        if ((!hasAudio || audioPosition.exhausted) && (!hasVideo || videoPosition.exhausted)) {
            return DemuxResult::Finished;
        }

        if (duration > 0 && decodedPosition >= duration) {
            return Finish(false);
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
        cluster = bestCluster;

        // Early exit: we know we're past the end already.
        if (isLastCluster && duration > 0 && targetPosition >= duration) {
            decodedPosition = targetPosition;
            return Finish(true);
        }

        // A track can start later than the first cluster.
        const auto audioLocated = hasAudio && (audioPosition.entry != nullptr || LocateTrack(audioPosition, TrackType::Audio, selectedAudioTrackIndex));
        const auto videoLocated = hasVideo && (videoPosition.entry != nullptr || LocateTrack(videoPosition, TrackType::Video, selectedVideoTrackIndex));

        decltype(TrackPosition::entry) initialAudio = audioPosition.entry;
        decltype(TrackPosition::entry) initialVideo = videoPosition.entry;
        decltype(TrackPosition::entry) finalAudio = initialAudio;
        decltype(TrackPosition::entry) finalVideo = initialVideo;

        if (audioLocated) {
            finalAudio = AdvanceTrack(initialAudio, TrackType::Audio, targetPosition, selectedAudioTrackIndex);
        }
        if (videoLocated) {
            finalVideo = AdvanceTrack(initialVideo, TrackType::Video, targetPosition, selectedVideoTrackIndex);
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
            return Finish(true);
        }

        if (duration > 0 && decodedPosition >= duration) {
            return Finish(false);
        }

        return DemuxResult::Success;
    }

    DemuxResult SourceDecoder::Impl::Finish(const bool reachedLastCluster) {
        // Decode outputs up to, not including, each track's position, so the packets from there on are still owed.
        const auto timeLimit = reachedLastCluster ? std::numeric_limits<double>::infinity() : duration;
        if (audioDecoder && !audioTracks.empty()) {
            OutputRemainingPackets(audioPosition, TrackType::Audio, selectedAudioTrackIndex, timeLimit);
        }
        if (videoDecoder && !videoTracks.empty()) {
            const auto flushDecoder = !videoPosition.exhausted;
            OutputRemainingPackets(videoPosition, TrackType::Video, selectedVideoTrackIndex, timeLimit);
            if (flushDecoder) {
                videoDecoder->Flush();
            }
        }
        bytesNeeded = 0;
        return DemuxResult::Finished;
    }

    void SourceDecoder::Impl::OutputRemainingPackets(TrackPosition &position, const TrackType type, const int trackIndex, const double timeLimit) {
        if (position.exhausted) {
            return;
        }
        position.exhausted = true;

        if (position.entry == nullptr) {
            LocateTrack(position, type, trackIndex);
        }
        auto entry = position.entry;
        while (entry != nullptr && !entry->EOS()) {
            auto entryCluster = entry->GetCluster();
            const auto block = entry->GetBlock();
            if (!IsBlockAvailable(entry)) {
                reader.focus = entryCluster->m_element_start;
                if (!Request(block->m_start + block->m_size)) {
                    return;
                }
            }
            if (IsBlockOfTrack(block, type, trackIndex)) {
                if (nanoSecsToSecs(block->GetTime(entryCluster)) > timeLimit) {
                    return;
                }
                if (type == TrackType::Video) {
                    OutputVideoBlock(entry);
                } else {
                    OutputAudioBlock(entry);
                }
            }

            // The block to carry on from within entryCluster, none once the walk has moved to a cluster it has no block of yet
            auto from = entry;
            const mkvparser::BlockEntry *next = nullptr;
            entryCluster->GetNext(from, next);
            while (next == nullptr || next->EOS()) {
                // The cluster may have more blocks than have arrived.
                if (const auto extent = MeasureCluster(entryCluster); !extent.complete) {
                    if (!Request(extent.needed)) {
                        return;
                    }
                } else {
                    const mkvparser::Cluster* nextCluster = nullptr;
                    if (NextCluster(entryCluster, nextCluster) != ClusterResult::Found) {
                        return;
                    }
                    entryCluster = nextCluster;
                    from = nullptr;
                }
                if (from != nullptr) {
                    entryCluster->GetNext(from, next);
                } else {
                    entryCluster->GetFirst(next);
                }
            }
            entry = next;
        }
    }

    DemuxResult SourceDecoder::Impl::Seek(double timestamp) {
        if (const auto prepared = Prepare(); prepared != DemuxResult::Success) {
            return prepared;
        }

        // Audio has to start early enough to cover its pre-roll.
        auto startTime = timestamp;
        if (!audioTracks.empty()) {
            startTime -= std::max(0.0, audioTracks[selectedAudioTrackIndex].seekPreRoll);
        }

        // The rest of the seek only looks at clusters from startCluster to the one that holds timestamp.
        auto startCluster = segment->GetFirst();
        const mkvparser::Cluster* seekCluster = nullptr;
        auto result = ClusterResult::NeedMoreData;
        ByteRange cueNeeded{};
        if (const auto cueCluster = FindCueCluster(startTime)) {
            result = FindBestCluster(timestamp, cueCluster, seekCluster);
            if (result != ClusterResult::NeedMoreData) {
                startCluster = cueCluster;
            }
        }
        if (result == ClusterResult::NeedMoreData) {
            cueNeeded = {neededStart, bytesNeeded};
            // Without the index, or the part of the file it points at, everything before timestamp has to be here.
            if (FindBestCluster(timestamp, startCluster, seekCluster) == ClusterResult::NeedMoreData) {
                // The walk only asks for its next step, the index knows where the seek ends up.
                if (cueNeeded.end > 0) {
                    neededStart = cueNeeded.start;
                    bytesNeeded = cueNeeded.end;
                }
                return DemuxResult::IncompleteCluster;
            }
        }
        bytesNeeded = 0;

        // The block of a track that covers time, or none when the cluster has no block of that track.
        const auto place = [this](const mkvparser::Cluster* target, const TrackType type, const double time, const int trackIndex) {
            TrackPosition position{};
            position.cluster = target;
            target->GetFirst(position.entry);
            FindBlockOfType(position.entry, type, time, trackIndex);
            if (position.entry != nullptr) {
                position.UseBlockTime();
            }
            return position;
        };

        TrackPosition preRoll{};
        TrackPosition audio{};
        if (!audioTracks.empty()) {
            const auto &track = audioTracks[selectedAudioTrackIndex];
            // TODO: Look into the right way to do this
            if (track.seekPreRoll > 0) {
                const mkvparser::Cluster* preRollCluster{};
                const auto preRollTime = timestamp - track.seekPreRoll;
                FindBestCluster(preRollTime, startCluster, preRollCluster);
                preRoll = place(preRollCluster, TrackType::Audio, preRollTime, selectedAudioTrackIndex);
            }
            audio = place(seekCluster, TrackType::Audio, timestamp, selectedAudioTrackIndex);
        }

        TrackPosition key{};
        TrackPosition video{};
        if (!videoTracks.empty()) {
            video = place(seekCluster, TrackType::Video, timestamp, selectedVideoTrackIndex);
            TrackPosition first{};
            first.cluster = startCluster;
            // LocateTrack looks no further than the current cluster.
            const auto current = cluster;
            cluster = seekCluster;
            if (video.entry != nullptr && LocateTrack(first, TrackType::Video, selectedVideoTrackIndex)) {
                if (const auto keyEntry = FindRecentKeyBlock(timestamp, first.entry, video.entry); keyEntry != nullptr) {
                    key.cluster = keyEntry->GetCluster();
                    key.entry = keyEntry;
                    key.UseBlockTime();
                }
            }
            cluster = current;
        }

        decodedPosition = timestamp;
        cluster = seekCluster;

        if (audioDecoder && !audioTracks.empty()) {
            audioDecoder->Reset();
            audioPosition = audio;
            if (preRoll.entry != nullptr && audioPosition.entry != nullptr) {
                OutputAudioPackets(preRoll, audioPosition);
            }
        }

        if (videoDecoder && !videoTracks.empty()) {
            videoDecoder->Reset();
            videoPosition = video;
            if (key.entry != nullptr) {
                OutputVideoPackets(key, videoPosition);
            }
        }

        return DemuxResult::Success;
    }

    void SourceDecoder::Impl::ReleaseConsumed() {
        if (parseState != ParseState::Ready || cluster == nullptr) {
            return;
        }
        // Clusters are where reading picks up again, so nothing before the earliest one in use is read.
        auto before = cluster->m_element_start;
        for (const auto position : {&audioPosition, &videoPosition}) {
            if (position->cluster != nullptr) {
                before = std::min(before, position->cluster->m_element_start);
            }
        }
        if (before != released) {
            released = before;
            reader.source->ReleaseBefore(before);
        }
    }

    void SourceDecoder::Impl::OutputVideoPackets(const TrackPosition &start, const TrackPosition &end) {
        BlockEntries entries{start, end};
        for (const auto entry : entries) {
            OutputVideoBlock(entry);
        }
    }

    void SourceDecoder::Impl::OutputVideoBlock(const mkvparser::BlockEntry *entry) {
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

    void SourceDecoder::Impl::OutputAudioPackets(const TrackPosition &start, const TrackPosition &end) {
        const BlockEntries entries{start, end};
        for (const auto entry : entries) {
            OutputAudioBlock(entry);
        }
    }

    void SourceDecoder::Impl::OutputAudioBlock(const mkvparser::BlockEntry *entry) {
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

    bool SourceDecoder::Impl::IsBlockOfTrack(const mkvparser::Block *block, const TrackType type, const int trackIndex) const {
        const auto trackNumber = static_cast<int>(block->GetTrackNumber());
        const auto trackType = trackNumbersToTrackTypes.find(trackNumber);
        return trackType != trackNumbersToTrackTypes.end() && trackType->second == type
            && trackNumbersToTrackIndexes.at(trackNumber) == trackIndex;
    }

    bool SourceDecoder::Impl::FindBlockOfType(const mkvparser::BlockEntry *&start, TrackType type, double time,
                                              int trackIndex) {
        const mkvparser::BlockEntry *initial = start;
        // GetNext gives null at the end of a cluster and where its data stops for now.
        while (start != nullptr && !start->EOS()) {
            auto track = static_cast<int>(start->GetBlock()->GetTrackNumber());

            const mkvparser::BlockEntry *next = nullptr;
            start->GetCluster()->GetNext(start, next);

            if (!IsBlockOfTrack(start->GetBlock(), type, trackIndex)) {
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

    SourceDecoder::Impl::~Impl() = default;

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
