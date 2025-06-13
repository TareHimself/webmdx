#include "webmd/SourceDecoder.h"

#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include "webm/mkvparser/mkvreader.h"
#include "webmd/TrackType.h"

namespace wd {

    double toSeconds(long long nanoseconds) {
        return static_cast<double>(nanoseconds) / 1e9;
    }

    struct SourceReader final : mkvparser::IMkvReader {
        SourceReader() = default;
        explicit SourceReader(const std::shared_ptr<ISource>& source);

        int Read(long long position, long length, unsigned char* buffer) override;

        int Length(long long* total, long long* available) override;
    private:

        std::shared_ptr<ISource> _source;
    };

    struct TrackPosition {
        const mkvparser::Cluster* cluster{};
        const mkvparser::BlockEntry* entry{};
        bool isFirstDecode{true};
        double seconds{};
        void UpdateSeconds();
    };

    struct SourceDecoder::Impl {
        SourceReader reader{};
        long long byteDecodePosition = 0;
        mkvparser::Segment * segment{};
        double duration{};
        double position{};
        long long timecodeScale = 0;
        std::vector<AudioTrack> audioTracks{};
        std::vector<VideoTrack> videoTracks{};
        std::unordered_map<int,TrackType> trackNumbersToTrackTypes{};
        std::unordered_map<int,int> trackNumbersToTrackIndexes{};
        int selectedAudioTrack = 0;
        int selectedVideoTrack = 0;
        const mkvparser::Cluster* cluster{};
        TrackPosition audioPosition{};
        TrackPosition videoPosition{};
        long long lastDecodedFramePos = -1;

        vpx_codec_ctx_t vpxContext{};

        std::optional<FrameCallback> _frameCallback{};
        std::optional<AudioCallback> _audioCallback{};

        void Init();
        void InitVideoDecoder();
        void InitAudioDecoder();
        bool Decode(double seconds);
        bool DecodeVideo(TrackPosition& start,TrackPosition& end);

        bool DecodeAudio(TrackPosition& start,TrackPosition& end);
        TrackType GetEntryTrackType(const mkvparser::BlockEntry* entry);
        TrackType GetBlockTrackType(const mkvparser::Block* block);
        bool FindBlockOfType(const mkvparser::BlockEntry*& start, TrackType type, double time, int trackIndex);

        ~Impl();
    };



    SourceReader::SourceReader(const std::shared_ptr<ISource> &source) {
        _source = source;
    }

    int SourceReader::Read(long long position, long length, unsigned char *buffer) {
        _source->Read(position,length,buffer);
        return 0;
    }

    int SourceReader::Length(long long *total, long long *available) {
        *total = _source->GetLength();
        *available = _source->GetAvailable();
        return 0;
    }

    void TrackPosition::UpdateSeconds() {
        seconds = toSeconds(entry->GetBlock()->GetTime(cluster));
    }

    void SourceDecoder::Impl::Init() {
        mkvparser::EBMLHeader header;
        header.Parse(&reader,byteDecodePosition);
        long long ret = mkvparser::Segment::CreateInstance(&reader,byteDecodePosition, segment);
        segment->Load();
        const auto segmentInfo = segment->GetInfo();
        duration = static_cast<double>(segmentInfo->GetDuration()) / 1e9;
        timecodeScale = segmentInfo->GetTimeCodeScale();
        const auto tracks = segment->GetTracks();
        const auto numTracks = tracks->GetTracksCount();

        bool seekSupported = true;
        auto cues  = segment->GetCues();
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
                    const auto asAudio = dynamic_cast<const mkvparser::AudioTrack*>(track);
                    AudioTrack audioTrack{};
                    audioTrack.channels = static_cast<int>(asAudio->GetChannels());
                    audioTrack.sampleRate = static_cast<int>(asAudio->GetSamplingRate());
                    audioTrack.bitDepth = static_cast<int>(asAudio->GetBitDepth());
                    audioTrack.codecDelay = static_cast<double>(asAudio->GetCodecDelay());
                    audioTrack.seekPreRoll = static_cast<double>(asAudio->GetSeekPreRoll());
                    trackNumbersToTrackIndexes.emplace(trackNumber,audioTracks.size());
                    audioTracks.push_back(audioTrack);
                    trackNumbersToTrackTypes.emplace(trackNumber,TrackType::Audio);
                }
                    break;
                case mkvparser::Track::kVideo: {
                    const auto trackNumber = track->GetNumber();
                    const auto asVideo = dynamic_cast<const mkvparser::VideoTrack*>(track);


                    VideoTrack videoTrack{};
                    videoTrack.width = static_cast<int>(asVideo->GetWidth());
                    videoTrack.height = static_cast<int>(asVideo->GetHeight());

                    auto codec = asVideo->GetCodecId();
                    if (strcmp(codec,"V_VP9") == 0) {
                        videoTrack.codec = VideoCodec::Vpx9;
                    }
                    else {
                        videoTrack.codec = VideoCodec::Vpx8;
                    }

                    trackNumbersToTrackIndexes.emplace(trackNumber,videoTracks.size());
                    videoTracks.push_back(videoTrack);
                    trackNumbersToTrackTypes.emplace(trackNumber,TrackType::Video);




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
        FindBlockOfType(videoPosition.entry,TrackType::Video,position,selectedVideoTrack);
        if (vpxContext.iface != nullptr) {
            vpx_codec_destroy(&vpxContext);
            vpxContext = {};
        }

        auto &selectedTrack = videoTracks[selectedVideoTrack];
        vpx_codec_dec_cfg_t cfg = {};
        cfg.h = selectedTrack.height;
        cfg.w = selectedTrack.width;
        cfg.threads = std::thread::hardware_concurrency();

        switch (selectedTrack.codec) {
            case VideoCodec::Vpx8: {
                if (vpx_codec_dec_init(&vpxContext,vpx_codec_vp8_dx(),nullptr,0)) {
                    std::cerr << "Failed to initialize libvpx decoder: " << vpx_codec_error(&vpxContext) << std::endl;
                    vpxContext = {};
                }
            }
                break;
            case VideoCodec::Vpx9: {
                if (vpx_codec_dec_init(&vpxContext,vpx_codec_vp9_dx(),nullptr,0)) {
                    std::cerr << "Failed to initialize libvpx decoder: " << vpx_codec_error(&vpxContext) << std::endl;
                    vpxContext = {};
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
        FindBlockOfType(audioPosition.entry,TrackType::Audio,position,selectedAudioTrack);
    }


    bool SourceDecoder::Impl::Decode(double seconds) {
        auto hasAudio = !audioTracks.empty();
        auto hasVideo = !videoTracks.empty();

        if (!hasAudio && !hasVideo) {
            return true;
        }

        position = std::min(position + seconds,duration);
        auto count = segment->GetCount();
        auto initialCluster = cluster;
        decltype(TrackPosition::entry) initialAudio = audioPosition.entry;
        decltype(TrackPosition::entry) initialVideo = videoPosition.entry;

        decltype(TrackPosition::entry) finalAudio = initialAudio;
        decltype(TrackPosition::entry) finalVideo = initialVideo;

        auto targetCluster = initialCluster;
        while (targetCluster != nullptr) {
            const auto start = toSeconds(targetCluster->GetFirstTime());
            const auto end = toSeconds(targetCluster->GetLastTime());

            // We found the correct cluster
            if (start <= position && position <= end) {
                break;
            }

            targetCluster = segment->GetNext(targetCluster);
        }

        if (targetCluster == nullptr) {
            return false;
        }

        if (initialCluster != targetCluster) {
            targetCluster->GetFirst(finalAudio);
            targetCluster->GetFirst(finalVideo);
        }

        if (hasAudio) {
            FindBlockOfType(finalAudio,TrackType::Audio,position,selectedAudioTrack);
        }
        if (hasVideo) {
            FindBlockOfType(finalVideo,TrackType::Video,position,selectedVideoTrack);
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
            audioResult = DecodeAudio(initial,audioPosition);
        }

        if (hasVideo && initialVideo != finalVideo) {
            auto initial = videoPosition;
            videoPosition.cluster = cluster;
            videoPosition.entry = finalVideo;
            videoPosition.isFirstDecode = false;
            videoPosition.UpdateSeconds();
            videoResult = DecodeVideo(initial,videoPosition);
        }

        return audioResult && videoResult;
    }

    static uint8_t clamp(int value) {
        return static_cast<uint8_t>(std::min(255, std::max(0, value)));
    }

    void ConvertI420ToRGBA(const vpx_image_t* img, std::vector<uint8_t>& outRGBA) {
        const int width = img->d_w;
        const int height = img->d_h;
        outRGBA.resize(width * height * 4);  // RGBA = 4 bytes per pixel

        const uint8_t* y_plane = img->planes[0];
        const uint8_t* u_plane = img->planes[1];
        const uint8_t* v_plane = img->planes[2];
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
                outRGBA[index + 0] = clamp(r);   // R
                outRGBA[index + 1] = clamp(g);   // G
                outRGBA[index + 2] = clamp(b);   // B
                outRGBA[index + 3] = 255;        // A
            }
        }
    }

    bool SourceDecoder::Impl::DecodeVideo(TrackPosition &start, TrackPosition &end) {
        auto cluster = start.cluster;
        auto entry = start.entry;
        // if (!start.isFirstDecode) {
        //     decltype(TrackPosition::entry) prev = entry;
        //     cluster->GetNext(prev,entry);
        // }
        auto trackNumber = entry->GetBlock()->GetTrackNumber();
        std::vector<unsigned char> buffer{};
        while (cluster != nullptr) {
            while (entry != nullptr && entry->GetKind() != mkvparser::BlockEntry::kBlockEOS) {
                auto block = entry->GetBlock();
                const mkvparser::BlockEntry * nextEntry;
                cluster->GetNext(entry,nextEntry);

                if (block->GetTrackNumber() == trackNumber) {
                    auto frameCount = block->GetFrameCount();
                    for (auto  i = 0; i < frameCount; i++) {
                        auto frame = block->GetFrame(i);
                        if (frame.pos == lastDecodedFramePos) {
                            continue;
                        }
                        if (buffer.size() < frame.len) {
                            buffer.resize(frame.len);
                        }
                        frame.Read(&reader,buffer.data());
                        auto decodeError = vpx_codec_decode(&vpxContext,buffer.data(),frame.len,nullptr,0);
                        lastDecodedFramePos = frame.pos;
                        if (decodeError != VPX_CODEC_OK) {
                            std::cerr << "Decode error " << vpx_codec_error(&vpxContext) << std::endl;
                            InitVideoDecoder();
                            return false;
                        }
                    }
                }
                if (entry == end.entry) {
                    break;
                }
                entry = nextEntry;
            }

            if (cluster == end.cluster) {
                cluster = nullptr;
            }
            else {
                cluster = segment->GetNext(cluster);
                cluster->GetFirst(entry);
            }
        }
        if (_frameCallback.has_value()) {
            vpx_codec_iter_t iter = nullptr;
            vpx_image_t* frame = vpx_codec_get_frame(&vpxContext, &iter);

            std::vector<uint8_t> outFrame{};
            ConvertI420ToRGBA(frame,outFrame);
            (*_frameCallback)(end.seconds,outFrame);
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

    bool SourceDecoder::Impl::FindBlockOfType(const mkvparser::BlockEntry *&start, TrackType type, double time, int trackIndex) {
        const mkvparser::BlockEntry * initial = start;
        while (start->GetKind() != mkvparser::BlockEntry::kBlockEOS) {
            auto track = static_cast<int>(start->GetBlock()->GetTrackNumber());
            auto trackType = trackNumbersToTrackTypes[track];

            const mkvparser::BlockEntry * next;
            start->GetCluster()->GetNext(start,next);

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

            auto startTime = toSeconds(start->GetBlock()->GetTime(start->GetCluster()));

            if (startTime > time) {
                break;
            }

            if (startTime == time) {
                return true;
            }

            while (next != nullptr && next->GetBlock()->GetTrackNumber() != track) {
                const mkvparser::BlockEntry * temp;
                next->GetCluster()->GetNext(next,temp);
                next = temp;
            }

            // If next is null we are as close as possible within this cluster
            if (next == nullptr) {
                return true;
            }

            auto nextTime = toSeconds(next->GetBlock()->GetTime(next->GetCluster()));

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
            vpx_codec_destroy(&vpxContext);
        }
    }


    SourceDecoder::SourceDecoder() {
        _impl = new Impl();
    }

    SourceDecoder::~SourceDecoder() {
        delete _impl;
        _impl = nullptr;
    }

    void SourceDecoder::SetSource(const std::shared_ptr<ISource> &source) {
        _impl->reader = SourceReader(source);
        _impl->Init();

    }

    void SourceDecoder::Decode(double seconds) {
        _impl->Decode(seconds);
    }

    double SourceDecoder::GetDuration() const {
        return _impl->duration;
    }

    double SourceDecoder::GetPosition() const {
        return _impl->position;
    }

    int SourceDecoder::GetAudioTrackCount() const {
        return _impl->audioTracks.size();
    }

    int SourceDecoder::GetVideoTrackCount() const {
        return _impl->videoTracks.size();
    }

    AudioTrack SourceDecoder::GetAudioTrack(int index) const {
        return _impl->audioTracks[index];
    }

    VideoTrack SourceDecoder::GetVideoTrack(int index) const {
        return _impl->videoTracks[index];
    }

    void SourceDecoder::SetFrameCallback(const FrameCallback& callback) {
        _impl->_frameCallback = callback;
    }

    void SourceDecoder::SetAudioCallback(const AudioCallback& callback) {
        _impl->_audioCallback = callback;
    }
}
