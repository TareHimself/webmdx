#include <iostream>
#include <memory>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "webmd/FileSource.h"
#include "webmd/SourceDecoder.h"

void testDecoder(const char* path) {
    auto source = std::make_shared<wd::FileSource>(path);
    auto decoder = std::make_shared<wd::SourceDecoder>();
    decoder->SetSource(source);
    std::vector<std::uint8_t> latestFrame{};
    auto track = decoder->GetVideoTrack();
    decoder->SetVideoCallback([&](double time, const std::shared_ptr<wd::IDecodedVideoFrame>& frame) {
        frame->ToRgba(latestFrame);
        stbi_write_png("./latest.png",track.width,track.height,4,latestFrame.data(),track.width * 4);
        std::cout << "Got Video " << time << std::endl;
    });
    decoder->SetAudioCallback([](double time, const std::span<float>& frame) {
        std::cout << "Got Audio " << time << std::endl;
    });
    latestFrame.resize(track.width * track.height * 4);
    auto decodeResult = decoder->Decode(30);
    while (decodeResult != wd::DecodeResult::Finished) {
        decodeResult = decoder->Decode(1);
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