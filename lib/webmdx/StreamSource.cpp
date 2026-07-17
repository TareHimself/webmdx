#include "webmdx/StreamSource.h"
#include <cstring>
#include <stdexcept>

namespace wdx {
    void StreamSource::Append(std::span<const std::uint8_t> data) {
        _buffer.insert(_buffer.end(), data.begin(), data.end());
    }

    void StreamSource::SetEndOfStream() {
        _eos = true;
    }

    void StreamSource::Read(const std::int64_t& pos, std::span<std::uint8_t> data) {
        const auto end = pos + static_cast<std::int64_t>(data.size());
        if (end > static_cast<std::int64_t>(_buffer.size())) {
            throw std::out_of_range("StreamSource::Read past available data");
        }
        std::memcpy(data.data(), _buffer.data() + pos, data.size());
    }

    std::int64_t StreamSource::GetLength() const {
        return _eos ? static_cast<std::int64_t>(_buffer.size()) : -1;
    }

    std::int64_t StreamSource::GetAvailable() const {
        return static_cast<std::int64_t>(_buffer.size());
    }

    void StreamSource::MakeAvailable(const std::uint64_t&) {
        // Data is pushed externally via Append; nothing to do here.
    }
}
