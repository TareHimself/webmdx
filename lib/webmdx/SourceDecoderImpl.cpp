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
        }

        reader = SourceReader(source);
        mkvparser::EBMLHeader header;
        header.Parse(&reader, byteDecodePosition);
        long long ret = mkvparser::Segment::CreateInstance(&reader, byteDecodePosition, segment);
        segment->Load();
        const auto segmentInfo = segment->GetInfo();

        if (const auto rawDuration = segmentInfo->GetDuration(); rawDuration == -1) {
            duration = nanoSecsToSecs(segment->GetLast()->GetTime());
        }
        else {
            duration = nanoSecsToSecs(rawDuration);
        }

        timecodeScale = segmentInfo->GetTimeCodeScale();
        const auto tracks = segment->GetTracks();
        const auto numTracks = tracks->GetTracksCount();

        bool seekSupported = false;
        const auto cues = segment->GetCues();
        if (cues != nullptr) {
            seekSupported = true;
            while (!cues->DoneParsing()) {
                if (!cues->LoadCuePoint()) {
                    seekSupported = false;
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
                    //asVideo
                    auto codec = asVideo->GetCodecId();
                    if (strcmp(codec, "V_VP9") == 0) {
                        videoTrack.codec = VideoCodec::Vpx9;
                    } else if (strcmp(codec, "V_VP8") == 0) {
                        videoTrack.codec = VideoCodec::Vpx8;
                    }else if (strcmp(codec, "V_AV1") == 0) {
                        videoTrack.codec = VideoCodec::Av1;
                    }

                    trackNumbersToTrackIndexes.emplace(trackNumber, videoTracks.size());
                    videoTracks.push_back(videoTrack);
                    trackNumbersToTrackTypes.emplace(trackNumber, TrackType::Video);


                    if (seekSupported) {
                        auto cue = cues->GetFirst();
                        while (cue != nullptr) {
                            auto timeSecs = cue->Find(asVideo);
                            auto block = timeSecs->m_block;
                            auto pos = timeSecs->m_pos;

                            cue = cues->GetNext(cue);
                        }
                    }
                }
                break;
                default:
                    break;
            }
        }
        cluster = segment->GetFirst();

        if (!audioTracks.empty()) {
            InitAudioDecoder();
        }

        if (!videoTracks.empty()) {
            InitVideoDecoder();
        }

    }

    void SourceDecoder::Impl::InitVideoDecoder() {
        lastDecodedAudioPos = -1;
        videoPosition = {};
        videoPosition.cluster = cluster;
        cluster->GetFirst(videoPosition.entry);
        FindBlockOfType(videoPosition.entry, TrackType::Video, decodedPosition, selectedVideoTrackIndex);
        const auto &track = videoTracks[selectedVideoTrackIndex];
        videoDecoder = IVideoDecoder::Create(track);
    }

    void SourceDecoder::Impl::InitAudioDecoder() {
        lastDecodedVideoPos = -1;
        audioPosition = {};
        audioPosition.cluster = cluster;
        cluster->GetFirst(audioPosition.entry);
        FindBlockOfType(audioPosition.entry, TrackType::Audio, decodedPosition, selectedAudioTrackIndex);
        const auto &track = audioTracks[selectedAudioTrackIndex];
        audioDecoder = IAudioDecoder::Create(track);
    }

    bool SourceDecoder::Impl::FindBestCluster(double timestamp, const mkvparser::Cluster *start,
        const mkvparser::Cluster *&best) const {
        auto isLast = false;
        auto targetCluster = start;
        do {
            if (timestamp <= nanoSecsToSecs(targetCluster->GetLastTime())) {
                break;
            }

            const auto next = segment->GetNext(targetCluster);

            if (next == nullptr || next->EOS()) {
                isLast = true;
                break;
            }

            targetCluster = next;
        } while (true);
        best = targetCluster;
        return isLast;
    }
    DecodeResult SourceDecoder::Impl::Decode(const double seconds) {
        if (decodedPosition >= duration) {
            return DecodeResult::Finished;
        }

        const auto hasAudio = audioDecoder && !audioTracks.empty();
        const auto hasVideo = videoDecoder && !videoTracks.empty();

        if (!hasAudio && !hasVideo) {
            throw NoTracksAvailableException();
        }

        decodedPosition = std::min(decodedPosition + seconds, duration);
        auto initialCluster = cluster;
        auto isLastCluster = false;
        decltype(TrackPosition::entry) initialAudio = audioPosition.entry;
        decltype(TrackPosition::entry) initialVideo = videoPosition.entry;
        decltype(TrackPosition::entry) finalAudio = initialAudio;
        decltype(TrackPosition::entry) finalVideo = initialVideo;

        isLastCluster =  FindBestCluster(decodedPosition,cluster,cluster);

        if (isLastCluster && decodedPosition >= duration) {
            return DecodeResult::Finished;
        }

        if (initialCluster != cluster) {
            cluster->GetFirst(finalAudio);
            cluster->GetFirst(finalVideo);
        }

        if (hasAudio) {
            FindBlockOfType(finalAudio, TrackType::Audio, decodedPosition, selectedAudioTrackIndex);
        }
        if (hasVideo) {
            FindBlockOfType(finalVideo, TrackType::Video, decodedPosition, selectedVideoTrackIndex);
        }

        if (hasAudio && initialAudio != finalAudio) {
            const auto initial = audioPosition;
            audioPosition.cluster = cluster;
            audioPosition.entry = finalAudio;
            audioPosition.UseBlockTime();
            DecodeAudio(initial, audioPosition);
        }

        if (hasVideo && initialVideo != finalVideo) {
            const auto initial = videoPosition;
            videoPosition.cluster = cluster;
            videoPosition.entry = finalVideo;
            videoPosition.UseBlockTime();
            DecodeVideo(initial, videoPosition);
        }

        if (isLastCluster && decodedPosition >= duration) {
            return DecodeResult::Finished;
        }


        return DecodeResult::Success;
    }

    void SourceDecoder::Impl::Seek(double timestamp) {
        decodedPosition = timestamp;
        FindBestCluster(timestamp,segment->GetFirst(),cluster);

        if (!audioTracks.empty()) {

            InitAudioDecoder();
            const auto &track = audioTracks[selectedAudioTrackIndex];
            if (track.seekPreRoll > 0) {
                const mkvparser::Cluster* preRollCluster{};

                auto preRollTime = timestamp - track.seekPreRoll;
                FindBestCluster(preRollTime,segment->GetFirst(),preRollCluster);
                audioPosition.SetCluster(preRollCluster);
                FindBlockOfType(audioPosition.entry, TrackType::Audio, preRollTime, selectedAudioTrackIndex);
                audioPosition.UseBlockTime();

                const auto initial = audioPosition;

                audioPosition.SetCluster(cluster);
                FindBlockOfType(audioPosition.entry, TrackType::Audio,timestamp, selectedAudioTrackIndex);
                audioPosition.UseBlockTime();
                DecodeAudio(initial, audioPosition,true);
            }
            else {
                audioPosition.SetCluster(cluster);
                FindBlockOfType(audioPosition.entry, TrackType::Audio,timestamp, selectedAudioTrackIndex);
                audioPosition.UseBlockTime();
            }
        }

        if (!videoTracks.empty()) {
            InitVideoDecoder();

            videoPosition.SetCluster(cluster);
            FindBlockOfType(videoPosition.entry, TrackType::Video,timestamp,selectedVideoTrackIndex);
            videoPosition.UseBlockTime();

            const mkvparser::BlockEntry * trackEntry;
            segment->GetFirst()->GetFirst(trackEntry);
            FindBlockOfType(trackEntry, TrackType::Video,0, selectedVideoTrackIndex);
            trackEntry = FindRecentKeyBlock(timestamp,trackEntry,videoPosition.entry);
            if (trackEntry != nullptr) {
                TrackPosition initial{};
                initial.cluster = trackEntry->GetCluster();
                initial.entry = trackEntry;
                initial.UseBlockTime();

                // Decode from the last key frame
                DecodeVideo(initial, videoPosition,true);
            }
        }
    }

    void SourceDecoder::Impl::DecodeVideo(const TrackPosition &start, const TrackPosition &end, bool decodeOnly) {
        BlockEntries entries{start, end};
        for (const auto entry : entries) {
            const auto block = entry->GetBlock();
            const auto frameCount = block->GetFrameCount();
            const auto time = nanoSecsToSecs(block->GetTime(entry->GetCluster()));
            for (auto i = 0; i < frameCount; i++) {
                auto frame = block->GetFrame(i);
                if (frame.pos == lastDecodedVideoPos) {
                    continue;
                }
                if (tempBuffer.size() < frame.len) {
                    tempBuffer.resize(frame.len);
                }
                frame.Read(&reader, tempBuffer.data());
                videoDecoder->Decode(std::span(tempBuffer.data(),frame.len),time);
                lastDecodedVideoPos = frame.pos;
                if (!decodeOnly && _videoCallback.has_value()) {
                    (*_videoCallback)(time,videoDecoder->GetFrame());
                }
            }
        }
    }

    void SourceDecoder::Impl::DecodeAudio(const TrackPosition &start, const TrackPosition &end, bool decodeOnly) {
        const BlockEntries entries{start, end};

        std::vector<float> samples{};
        const auto audioCallback = _audioCallback.has_value() ? &_audioCallback.value() : nullptr;
        for (const auto entry : entries) {
            const auto block = entry->GetBlock();
            const auto time = nanoSecsToSecs(block->GetTime(entry->GetCluster()));
            const auto frameCount = block->GetFrameCount();
            for (auto i = 0; i < frameCount; i++) {
                const auto frame = block->GetFrame(i);
                if (frame.pos == lastDecodedAudioPos) {
                    continue;
                }
                if (tempBuffer.size() < frame.len) {
                    tempBuffer.resize(frame.len);
                }
                frame.Read(&reader, tempBuffer.data());

                const auto samplesDecoded = audioDecoder->Decode(std::span(tempBuffer.data(),frame.len),samples, time);
                lastDecodedAudioPos = frame.pos;
                if (!decodeOnly && audioCallback) {
                    (*audioCallback)(time,std::span(samples.data(),samplesDecoded));
                }
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

    const mkvparser::BlockEntry * SourceDecoder::Impl::FindRecentKeyBlock(double timestamp,const mkvparser::BlockEntry* initialEntry,const mkvparser::BlockEntry* targetBlockEntry) {
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
}
