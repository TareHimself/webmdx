#include <iostream>
#include <memory>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "NetworkSource.h"
#include "webmdx/FileSource.h"
#include "webmdx/SourceDecoder.h"

void testDecoder(const char* path)
{
    // auto a = std::make_shared<NetworkSource>("https://dl11.webmfiles.org/big-buck-bunny_trailer-.webm");
    // auto b = std::make_shared<NetworkSource>("https://dl11.webmfiles.org/big-buck-bunny_trailer-.webm");
    //     a->MakeAvailable(a->GetLength());
    // while (b->GetLength() != b->GetAvailable())
    // {
    //     b->MakeAvailable(b->GetAvailable() + (49086481 / 5));
    // }
    //
    // std::vector<std::uint8_t> aData{};
    // std::vector<std::uint8_t> bData{};
    // aData.resize(a->GetAvailable());
    // bData.resize(b->GetAvailable());
    // a->Read(0,aData);
    // b->Read(0,bData);
    // for (auto i = 0; i < aData.size(); i++)
    // {
    //     if (aData[i] != bData[i])
    //     {
    //         std::cout << aData[i] << " != " << bData[i] << std::endl;
    //     }
    // }
    auto source = std::make_shared<wdx::FileSource>(path);
    auto decoder = std::make_shared<wdx::SourceDecoder>();
    decoder->SetSource(source);
    auto channels = 4;
    std::vector<std::uint8_t> latestFrame{};
    auto track = decoder->GetVideoTrack();
    decoder->SetVideoPacketCallback([&](const std::shared_ptr<wdx::Packet>& packet, wdx::IVideoDecoder* decoder)
    {
        // if (time > 2) {
        //     frame->ToRgba(latestFrame);
        //
        //     stbi_write_png("./latest.png",track.width,track.height,channels,latestFrame.data(),track.width * channels);
        //     std::cout << "" << std::endl;
        // }
        decoder->Decode(packet);
        std::cout << "Got Video " << packet->GetTime() << std::endl;
    });
    decoder->SetAudioPacketCallback([&](const std::shared_ptr<wdx::Packet>& packet, wdx::IAudioDecoder* decoder)
    {
        std::vector<float> out{};
        decoder->Decode(packet, out);
        std::cout << "Got Audio " << packet->GetTime() << std::endl;
    });
    latestFrame.resize(track.width * track.height * channels);
    auto decodeResult = decoder->Demux(0.1);
    while (decodeResult != wdx::DemuxResult::Finished)
    {
        decodeResult = decoder->Demux(0.1);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_webm_file>" << std::endl;
        return 1;
    }
    testDecoder(argv[1]);
    return 0;
}
