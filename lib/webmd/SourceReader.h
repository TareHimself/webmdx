#pragma once
#include <memory>
#include <webm/mkvparser/mkvparser.h>

#include "webmd/ISource.h"

namespace wd {
    struct SourceReader final : mkvparser::IMkvReader {
        SourceReader() = default;

        explicit SourceReader(const std::shared_ptr<ISource> &source);

        int Read(long long position, long length, unsigned char *buffer) override;

        int Length(long long *total, long long *available) override;

    private:
        std::shared_ptr<ISource> _source;
    };
}
