#include "SourceDecoderImpl.h"

#include <cstring>
#include <iostream>
#include <span>
#include <thread>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>

#include "BlockEntries.h"
#include "webmd/errors.h"
#include "webmd/utils.h"

namespace wd {
    void SourceDecoder::Impl::Init() {
        mkvparser::EBMLHeader header;
        header.Parse(&reader, byteDecodePosition);
        long long ret = mkvparser::Segment::CreateInstance(&reader, byteDecodePosition, segment);
        segment->Load();
        const auto segmentInfo = segment->GetInfo();
        duration = static_cast<double>(segmentInfo->GetDuration()) / 1e9;
        timecodeScale = segmentInfo->GetTimeCodeScale();
        const auto tracks = segment->GetTracks();
        const auto numTracks = tracks->GetTracksCount();

        bool seekSupported = true;
        auto cues = segment->GetCues();
        while (!cues->DoneParsing()) {
            if (!cues->LoadCuePoint()) {
                seekSupported = false;
                break;
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
                    audioTrack.seekPreRoll = static_cast<double>(asAudio->GetSeekPreRoll());
                    audioTrack.codecPrivate = asAudio->GetCodecPrivate(audioTrack.codecPrivateSize);

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

        InitVideoDecoder();
        InitAudioDecoder();
    }


    void SourceDecoder::Impl::InitVideoDecoder() {
        videoPosition = {};
        videoPosition.cluster = cluster;
        cluster->GetFirst(videoPosition.entry);
        FindBlockOfType(videoPosition.entry, TrackType::Video, decodedPosition, selectedVideoTrackIndex);
        const auto &track = videoTracks[selectedVideoTrackIndex];
        videoDecoder = IVideoDecoder::Create(track);
    }

    void SourceDecoder::Impl::InitAudioDecoder() {
        audioPosition = {};
        audioPosition.cluster = cluster;
        cluster->GetFirst(audioPosition.entry);
        FindBlockOfType(audioPosition.entry, TrackType::Audio, decodedPosition, selectedAudioTrackIndex);
        const auto &track = audioTracks[selectedAudioTrackIndex];
        audioDecoder = IAudioDecoder::Create(track);
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

        auto targetCluster = initialCluster;
        while (targetCluster != nullptr && !targetCluster->EOS()) {
            const auto start = nanoSecsToSecs(targetCluster->GetFirstTime());
            const auto end = nanoSecsToSecs(targetCluster->GetLastTime());

            // break when we found the perfect cluster or the last cluster
            if (start <= decodedPosition && decodedPosition <= end) {
                break;
            }

            if (start <= decodedPosition && segment->GetNext(targetCluster)->EOS()) {
                isLastCluster = true;
                break;
            }

            targetCluster = segment->GetNext(targetCluster);
        }

        if ((targetCluster == nullptr || targetCluster->EOS())) {
            if (decodedPosition < duration) {
                throw ExpectedDataException();
            }
            return DecodeResult::Finished;
        }

        if (initialCluster != targetCluster) {
            targetCluster->GetFirst(finalAudio);
            targetCluster->GetFirst(finalVideo);
        }

        if (hasAudio) {
            FindBlockOfType(finalAudio, TrackType::Audio, decodedPosition, selectedAudioTrackIndex);
        }
        if (hasVideo) {
            FindBlockOfType(finalVideo, TrackType::Video, decodedPosition, selectedVideoTrackIndex);
        }

        cluster = targetCluster;

        if (hasAudio && initialAudio != finalAudio) {
            const auto initial = audioPosition;
            audioPosition.cluster = cluster;
            audioPosition.entry = finalAudio;
            audioPosition.isFirstDecode = false;
            audioPosition.UpdateSeconds();
            DecodeAudio(initial, audioPosition);
        }

        if (hasVideo && initialVideo != finalVideo) {
            const auto initial = videoPosition;
            videoPosition.cluster = cluster;
            videoPosition.entry = finalVideo;
            videoPosition.isFirstDecode = false;
            videoPosition.UpdateSeconds();
            DecodeVideo(initial, videoPosition);
        }

        if (isLastCluster && decodedPosition >= duration) {
            return DecodeResult::Finished;
        }


        return DecodeResult::Success;
    }

    void SourceDecoder::Impl::DecodeVideo(const TrackPosition &start, const TrackPosition &end) {
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
            }
        }

        if (_videoCallback.has_value()) {
            (*_videoCallback)(end.seconds,videoDecoder->GetFrame());
        }
    }

    void SourceDecoder::Impl::DecodeAudio(const TrackPosition &start, const TrackPosition &end) {
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
                if (audioCallback) {
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

    SourceDecoder::Impl::~Impl() {
        delete segment;
        segment = nullptr;
    }
}
