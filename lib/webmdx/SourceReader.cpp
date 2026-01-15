#include "SourceReader.h"

namespace wdx {
    SourceReader::SourceReader(const std::shared_ptr<ISource> &inSource) {
        source = inSource;
    }

    int SourceReader::Read(long long position, long length, unsigned char *buffer) {
        std::span data{buffer, static_cast<std::span<unsigned char>::size_type>(length)};
        source->Read(position,data);
        return 0;
    }

    int SourceReader::Length(long long *total, long long *available) {
        *total = source->GetLength();
        *available = source->GetAvailable();
        return 0;
    }
}
