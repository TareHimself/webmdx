#include <iostream>
#include <memory>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "webmdx/FileSource.h"
#include "webmdx/SourceDecoder.h"

void testDecoder(const char* path) {
    auto source = std::make_shared<wdx::FileSource>(path);
    auto decoder = std::make_shared<wdx::SourceDecoder>();
    decoder->SetSource(source);
    auto channels = 4;
    std::vector<std::uint8_t> latestFrame{};
    auto track = decoder->GetVideoTrack();
    decoder->SetVideoCallback([&](double time, const std::shared_ptr<wdx::IDecodedVideoFrame>& frame) {
        // if (time > 75) {
        //     frame->ToRgba(latestFrame);
        //
        //     stbi_write_png("./latest.png",track.width,track.height,channels,latestFrame.data(),track.width * channels);
        //     std::cout << "" << std::endl;
        // }
        std::cout << "Got Video " << time << std::endl;
    });
    decoder->SetAudioCallback([](double time, const std::span<float>& frame) {
        std::cout << "Got Audio " << time << std::endl;
    });
    latestFrame.resize(track.width * track.height * channels);
    auto decodeResult = decoder->Decode(0.1);
    while (decodeResult != wdx::DecodeResult::Finished) {
        decodeResult = decoder->Decode(0.1);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_webm_file>" << std::endl;
        return 1;
    }
    testDecoder(argv[1]);
    return 0;
}