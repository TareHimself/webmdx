#pragma once
#include <memory>
#include "webmdx/ISource.h"

namespace wdx {
    struct SourceParser {
        SourceParser() = default;

        static std::shared_ptr<SourceParser> Parse(const std::shared_ptr<ISource>& source);
    private:
        std::shared_ptr<ISource> _source;
    };
}
