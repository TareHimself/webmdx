

#include <iostream>
#include <memory>

#include "webmd/FileSource.h"
#include "webmd/SourceDecoder.h"

void testDecoder() {
    const char* path = "/home/tare/Downloads/zzz.webm";
    auto source = std::make_shared<wd::FileSource>(path);
    auto decoder = std::make_shared<wd::SourceDecoder>();
    decoder->SetSource(source);
    decoder->SetFrameCallback([](double time,const std::vector<uint8_t>& frame) {
        std::cout << "Got Frame " << time << std::endl;
    });
    while (decoder->GetPosition() < decoder->GetDuration()) {
        decoder->Decode(0.1);
    }
}
int main(){
    testDecoder();
}