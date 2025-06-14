#include "SourceReader.h"

namespace wdx {
    SourceReader::SourceReader(const std::shared_ptr<ISource> &source) {
        _source = source;
    }

    int SourceReader::Read(long long position, long length, unsigned char *buffer) {
        _source->Read(position, length, buffer);
        return 0;
    }

    int SourceReader::Length(long long *total, long long *available) {
        *total = _source->GetLength();
        *available = _source->GetAvailable();
        return 0;
    }
}
