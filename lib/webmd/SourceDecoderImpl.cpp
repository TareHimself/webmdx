#include "SourceDecoderImpl.h"

#include <cstring>
#include <iostream>
#include <bits/std_thread.h>
#include <vpx/vp8dx.h>
#include <vpx/vpx_decoder.h>

#include "BlockIterable.h"
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
                    } else {
                        videoTrack.codec = VideoCodec::Vpx8;
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
        FindBlockOfType(videoPosition.entry, TrackType::Video, position, selectedVideoTrack);
        if (videoDecoder.iface != nullptr) {
            vpx_codec_destroy(&videoDecoder);
            videoDecoder = {};
        }

        auto &selectedTrack = videoTracks[selectedVideoTrack];
        vpx_codec_dec_cfg_t cfg = {};
        cfg.h = selectedTrack.height;
        cfg.w = selectedTrack.width;
        cfg.threads = std::thread::hardware_concurrency();

        switch (selectedTrack.codec) {
            case VideoCodec::Vpx8: {
                if (vpx_codec_dec_init(&videoDecoder, vpx_codec_vp8_dx(), nullptr, 0)) {
                    std::cerr << "Failed to initialize libvpx decoder: " << vpx_codec_error(&videoDecoder) << std::endl;
                    videoDecoder = {};
                }
            }
            break;
            case VideoCodec::Vpx9: {
                if (vpx_codec_dec_init(&videoDecoder, vpx_codec_vp9_dx(), nullptr, 0)) {
                    std::cerr << "Failed to initialize libvpx decoder: " << vpx_codec_error(&videoDecoder) << std::endl;
                    videoDecoder = {};
                }
            }
            break;
            default:
                break;
        }
    }

    void SourceDecoder::Impl::InitAudioDecoder() {
        audioPosition = {};
        audioPosition.cluster = cluster;
        cluster->GetFirst(audioPosition.entry);
        FindBlockOfType(audioPosition.entry, TrackType::Audio, position, selectedAudioTrack);
        if (audioDecoder) {
            opus_decoder_destroy(audioDecoder);
            audioDecoder = nullptr;
        }
        auto &selectedTrack = audioTracks[selectedAudioTrack];
        int error;
        audioDecoder = opus_decoder_create(selectedTrack.sampleRate, selectedTrack.channels, &error);
        if (error != OPUS_OK) {
            std::cerr << "Failed to initialize opus decoder: " << error << std::endl;
            if (audioDecoder) {
                opus_decoder_destroy(audioDecoder);
            }
        }
    }


    bool SourceDecoder::Impl::Decode(double seconds) {
        auto hasAudio = audioDecoder != nullptr && !audioTracks.empty();
        auto hasVideo = videoDecoder.iface != nullptr && !videoTracks.empty();

        if (!hasAudio && !hasVideo) {
            return true;
        }

        position = std::min(position + seconds, duration);
        auto count = segment->GetCount();
        auto initialCluster = cluster;
        decltype(TrackPosition::entry) initialAudio = audioPosition.entry;
        decltype(TrackPosition::entry) initialVideo = videoPosition.entry;

        decltype(TrackPosition::entry) finalAudio = initialAudio;
        decltype(TrackPosition::entry) finalVideo = initialVideo;

        auto targetCluster = initialCluster;
        while (targetCluster != nullptr && !targetCluster->EOS()) {
            const auto start = nanoSecsToSecs(targetCluster->GetFirstTime());
            const auto end = nanoSecsToSecs(targetCluster->GetLastTime());

            // We found the correct cluster
            if (start <= position && position <= end) {
                break;
            }

            targetCluster = segment->GetNext(targetCluster);
        }

        if (targetCluster == nullptr || targetCluster->EOS()) {
            return false;
        }

        if (initialCluster != targetCluster) {
            targetCluster->GetFirst(finalAudio);
            targetCluster->GetFirst(finalVideo);
        }

        if (hasAudio) {
            FindBlockOfType(finalAudio, TrackType::Audio, position, selectedAudioTrack);
        }
        if (hasVideo) {
            FindBlockOfType(finalVideo, TrackType::Video, position, selectedVideoTrack);
        }


        auto audioResult = true;
        auto videoResult = true;

        cluster = targetCluster;

        if (hasAudio && initialAudio != finalAudio) {
            auto initial = audioPosition;
            audioPosition.cluster = cluster;
            audioPosition.entry = finalAudio;
            audioPosition.isFirstDecode = false;
            audioPosition.UpdateSeconds();
            audioResult = DecodeAudio(initial, audioPosition);
        }

        if (hasVideo && initialVideo != finalVideo) {
            auto initial = videoPosition;
            videoPosition.cluster = cluster;
            videoPosition.entry = finalVideo;
            videoPosition.isFirstDecode = false;
            videoPosition.UpdateSeconds();
            videoResult = DecodeVideo(initial, videoPosition);
        }

        return audioResult && videoResult;
    }

    void ConvertI420ToRGBA(const vpx_image_t *img, std::vector<uint8_t> &outRGBA) {
        const int width = img->d_w;
        const int height = img->d_h;
        outRGBA.resize(width * height * 4); // RGBA = 4 bytes per pixel

        const uint8_t *y_plane = img->planes[0];
        const uint8_t *u_plane = img->planes[1];
        const uint8_t *v_plane = img->planes[2];
        const int y_stride = img->stride[0];
        const int u_stride = img->stride[1];
        const int v_stride = img->stride[2];

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int y_val = y_plane[y * y_stride + x];
                int u_val = u_plane[(y / 2) * u_stride + (x / 2)];
                int v_val = v_plane[(y / 2) * v_stride + (x / 2)];

                // Convert YUV to RGB
                int c = y_val - 16;
                int d = u_val - 128;
                int e = v_val - 128;

                int r = (298 * c + 409 * e + 128) >> 8;
                int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                int b = (298 * c + 516 * d + 128) >> 8;

                int index = (y * width + x) * 4;
                outRGBA[index + 0] = std::clamp<uint8_t>(r, 0, 255); // R
                outRGBA[index + 1] = std::clamp<uint8_t>(g, 0, 255); // G
                outRGBA[index + 2] = std::clamp<uint8_t>(b, 0, 255); // B
                outRGBA[index + 3] = 255; // A
            }
        }
    }

    bool SourceDecoder::Impl::DecodeVideo(TrackPosition &start, TrackPosition &end) {
        BlockIterable blocks{start, end};
        std::vector<unsigned char> buffer{};
        for (auto block: blocks) {
            const auto frameCount = block->GetFrameCount();

            for (auto i = 0; i < frameCount; i++) {
                auto frame = block->GetFrame(i);
                if (frame.pos == lastDecodedFramePos) {
                    continue;
                }
                if (buffer.size() < frame.len) {
                    buffer.resize(frame.len);
                }
                frame.Read(&reader, buffer.data());
                auto decodeError = vpx_codec_decode(&videoDecoder, buffer.data(), frame.len, nullptr, 0);
                lastDecodedFramePos = frame.pos;
                if (decodeError != VPX_CODEC_OK) {
                    std::cerr << "Decode error: " << vpx_codec_error(&videoDecoder) << std::endl;
                    InitVideoDecoder();
                    return false;
                }
            }
        }
        // auto cluster = start.cluster;
        // auto entry = start.entry;
        // // if (!start.isFirstDecode) {
        // //     decltype(TrackPosition::entry) prev = entry;
        // //     cluster->GetNext(prev,entry);
        // // }
        // auto trackNumber = entry->GetBlock()->GetTrackNumber();
        // std::vector<unsigned char> buffer{};
        // while (!cluster->EOS()) {
        //     while (entry != nullptr && !entry->EOS()) {
        //         const auto block = entry->GetBlock();
        //         const mkvparser::BlockEntry * nextEntry;
        //
        //         if (cluster->GetNext(entry,nextEntry) < 0) {
        //             return false;
        //         }
        //
        //         if (block->GetTrackNumber() != trackNumber) {
        //             entry = nextEntry;
        //             continue;
        //         }
        //
        //         const auto frameCount = block->GetFrameCount();
        //
        //         for (auto  i = 0; i < frameCount; i++) {
        //             auto frame = block->GetFrame(i);
        //             if (frame.pos == lastDecodedFramePos) {
        //                 continue;
        //             }
        //             if (buffer.size() < frame.len) {
        //                 buffer.resize(frame.len);
        //             }
        //             frame.Read(&reader,buffer.data());
        //             auto decodeError = vpx_codec_decode(&videoDecoder,buffer.data(),frame.len,nullptr,0);
        //             lastDecodedFramePos = frame.pos;
        //             if (decodeError != VPX_CODEC_OK) {
        //                 std::cerr << "Decode error: " << vpx_codec_error(&videoDecoder) << std::endl;
        //                 InitVideoDecoder();
        //                 return false;
        //             }
        //         }
        //
        //         if (entry == end.entry) {
        //             break;
        //         }
        //         entry = nextEntry;
        //     }
        //
        //     if (cluster == end.cluster) {
        //         break;
        //     }
        //
        //     cluster = segment->GetNext(cluster);
        //     cluster->GetFirst(entry);
        // }

        if (_videoCallback.has_value()) {
            vpx_codec_iter_t iter = nullptr;
            vpx_image_t *frame = vpx_codec_get_frame(&videoDecoder, &iter);

            std::vector<uint8_t> outFrame{};
            ConvertI420ToRGBA(frame, outFrame);
            (*_videoCallback)(end.seconds, outFrame);
        }

        return true;
    }

    bool SourceDecoder::Impl::DecodeAudio(TrackPosition &start, TrackPosition &end) {
        return true;
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
        if (!videoTracks.empty()) {
            vpx_codec_destroy(&videoDecoder);
        }
    }
}
