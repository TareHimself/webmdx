#include "webmdx/StreamSource.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace wdx {
    StreamSource::StreamSource(const bool dropReleased) : _dropReleased(dropReleased) {}

    void StreamSource::Append(std::span<const std::uint8_t> data) {
        _buffer.insert(_buffer.end(), data.begin(), data.end());
    }

    void StreamSource::SetEndOfStream() {
        _eos = true;
    }

    void StreamSource::Read(const std::int64_t& pos, std::span<std::uint8_t> data) {
        const auto end = pos + static_cast<std::int64_t>(data.size());
        if (pos < _start || end > GetAvailable()) {
            throw std::out_of_range("StreamSource::Read outside the held data");
        }
        std::memcpy(data.data(), _buffer.data() + (pos - _start), data.size());
    }

    std::int64_t StreamSource::GetLength() const {
        return _eos ? GetAvailable() : -1;
    }

    std::int64_t StreamSource::GetAvailable() const {
        return _start + static_cast<std::int64_t>(_buffer.size());
    }

    std::int64_t StreamSource::GetAvailableFrom(const std::int64_t& pos) const {
        return pos >= _start && pos < GetAvailable() ? GetAvailable() : pos;
    }

    void StreamSource::MakeAvailable(const std::uint64_t&) {
        // Data is pushed externally via Append; nothing to do here.
    }

    void StreamSource::ReleaseBefore(const std::int64_t& pos) {
        if (!_dropReleased) {
            return;
        }
        _released = std::max(_released, std::min(pos, GetAvailable()));
        // Erasing only once half the buffer can go keeps the cost of moving the rest down even.
        if (const auto drop = _released - _start; drop > 0 && drop >= static_cast<std::int64_t>(_buffer.size()) / 2) {
            _buffer.erase(_buffer.begin(), _buffer.begin() + drop);
            _start = _released;
        }
    }

    std::size_t StreamSource::GetBufferedSize() const {
        return _buffer.size();
    }
}
