#include <iostream>
#include <memory>

#include "webmd/FileSource.h"
#include "webmd/SourceDecoder.h"

void testDecoder(const char* path) {
    auto source = std::make_shared<wd::FileSource>(path);
    auto decoder = std::make_shared<wd::SourceDecoder>();
    decoder->SetSource(source);
    decoder->SetVideoCallback([](double time, const std::span<uint8_t>& frame) {
        std::cout << "Got Video " << time << std::endl;
    });
    decoder->SetAudioCallback([](double time, const std::span<float>& frame) {
        std::cout << "Got Audio " << time << std::endl;
    });
    while (decoder->GetPosition() < decoder->GetDuration()) {
        decoder->Decode(0.1);
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