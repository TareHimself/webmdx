#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "export.h"
namespace wdx {
    // A WebM file that may only be partly there. The decoder reads only below GetAvailable.
    class WEBMDX_API ISource {
    public:
        virtual ~ISource() = default;

        virtual void Read(const std::int64_t& pos, std::span<std::uint8_t> data) = 0;

        // Total size in bytes, -1 while unknown. The source has ended once GetAvailable reaches it.
        [[nodiscard]] virtual std::int64_t GetLength() const = 0;

        // Bytes from the start of the file that have arrived. Never shrinks.
        [[nodiscard]] virtual std::int64_t GetAvailable() const = 0;

        // The decoder needs the first size bytes. Fetch them (or more) if the source can, otherwise do nothing.
        virtual void MakeAvailable(const std::uint64_t& size) = 0;

        // The decoder won't read below pos again unless it seeks back. Dropping those bytes is up to the source.
        virtual void ReleaseBefore(const std::int64_t& pos) {}

        // Override both of these in a source that can hold pieces from anywhere in the file, which lets a seek
        // fetch only what it lands on.

        // End of the unbroken run of held bytes starting at pos, or pos when that byte is not held.
        [[nodiscard]] virtual std::int64_t GetAvailableFrom(const std::int64_t& pos) const {
            const auto available = GetAvailable();
            return pos < available ? available : pos;
        }

        // The decoder needs bytes [pos, pos + size).
        virtual void MakeRangeAvailable(const std::int64_t& pos, const std::uint64_t& size) {
            if (pos <= GetAvailable()) {
                MakeAvailable(static_cast<std::uint64_t>(pos) + size);
            }
        }
    };
}
