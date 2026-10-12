#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

#include "webmdx/FileSource.h"
#include "webmdx/SourceDecoder.h"

namespace
{
    struct Expected
    {
        int width;
        int height;
        std::size_t videoPackets;
    };

    int failures = 0;

    void check(const bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "FAILED: " << message << std::endl;
            failures++;
        }
    }

    void testDecoder(const char* path, const Expected& expected)
    {
        const auto source = std::make_shared<wdx::FileSource>(path);
        const auto decoder = std::make_shared<wdx::SourceDecoder>();
        decoder->SetSource(source);

        check(decoder->HasVideo(), "source has a video track");
        const auto track = decoder->GetVideoTrack();
        check(track.width == expected.width, "video width is " + std::to_string(expected.width) + ", got " + std::to_string(track.width));
        check(track.height == expected.height, "video height is " + std::to_string(expected.height) + ", got " + std::to_string(track.height));

        std::size_t videoPackets = 0;
        std::size_t videoFrames = 0;
        std::size_t audioPackets = 0;
        std::size_t audioSamples = 0;
        double lastVideoTime = -1;
        double lastAudioTime = -1;
        bool videoInOrder = true;
        bool audioInOrder = true;
        bool firstFrameHasContent = false;
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(track.width) * track.height * 4);

        const auto takeFrames = [&](wdx::IVideoDecoder* videoDecoder)
        {
            while (const auto frame = videoDecoder->GetFrame())
            {
                if (videoFrames == 0)
                {
                    frame->ToRgba(rgba);
                    for (const auto value : rgba)
                    {
                        firstFrameHasContent = firstFrameHasContent || value != rgba[0];
                    }
                }
                videoFrames++;
            }
        };

        decoder->SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>& packet, wdx::IVideoDecoder* videoDecoder)
        {
            videoInOrder = videoInOrder && packet->GetTime() >= lastVideoTime;
            lastVideoTime = packet->GetTime();
            videoPackets++;
            videoDecoder->Decode(packet);
            takeFrames(videoDecoder);
        });
        decoder->SetAudioPacketCallback([&](const std::shared_ptr<wdx::Packet>& packet, wdx::IAudioDecoder* audioDecoder)
        {
            audioInOrder = audioInOrder && packet->GetTime() >= lastAudioTime;
            lastAudioTime = packet->GetTime();
            audioPackets++;
            std::vector<float> pcm{};
            audioSamples += audioDecoder->Decode(packet, pcm);
        });

        auto result = decoder->Demux(0.1);
        while (result == wdx::DemuxResult::Success)
        {
            result = decoder->Demux(0.1);
        }

        check(result == wdx::DemuxResult::Finished, "demux reaches the end of a complete file");
        takeFrames(decoder->GetVideoDecoder());
        check(videoPackets == expected.videoPackets, "video packet count is " + std::to_string(expected.videoPackets) + ", got " + std::to_string(videoPackets));
        check(videoFrames == videoPackets, "every video packet decodes to a frame, got " + std::to_string(videoFrames));
        check(firstFrameHasContent, "first frame converts to a non-blank RGBA image");
        check(videoInOrder, "video packet times never go backwards");
        check(audioInOrder, "audio packet times never go backwards");
        if (decoder->HasAudio())
        {
            check(audioPackets > 0 && audioSamples > 0, "audio packets decode to samples");
        }

        std::cout << path << ": " << track.width << "x" << track.height << ", " << decoder->GetDuration() << "s, "
            << videoPackets << " video packets, " << videoFrames << " frames, "
            << audioPackets << " audio packets, " << audioSamples << " samples" << std::endl;
    }
}

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_webm_file> <width> <height> <video_packets>" << std::endl;
        return 2;
    }
    try
    {
        testDecoder(argv[1], {std::atoi(argv[2]), std::atoi(argv[3]), std::strtoull(argv[4], nullptr, 10)});
    }
    catch (const std::exception& e)
    {
        check(false, std::string("threw ") + typeid(e).name() + ": " + e.what());
    }
    return failures == 0 ? 0 : 1;
}
