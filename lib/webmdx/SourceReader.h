#pragma once
#include <memory>
#include <webm/mkvparser/mkvparser.h>

#include "webmdx/ISource.h"

namespace wdx {
    struct SourceReader final : mkvparser::IMkvReader {
        SourceReader() = default;

        explicit SourceReader(const std::shared_ptr<ISource> &inSource);

        int Read(long long position, long length, unsigned char *buffer) override;

        int Length(long long *total, long long *available) override;
        std::shared_ptr<ISource> source;
    };
}
