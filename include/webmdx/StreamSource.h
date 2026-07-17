#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include "ISource.h"
#include "export.h"

namespace wdx {
    // Push-based source for streaming/partial WebM data.
    // Call Append() as data arrives, then call Demux() on the SourceDecoder.
    // Demux returns IncompleteHeader or IncompleteCluster when more data is needed.
    // Call SetEndOfStream() when no more data will arrive.
    // Not thread-safe: do not call Append from one thread and Demux from another.
    class WEBMDX_API StreamSource final : public ISource {
    public:
        StreamSource() = default;

        void Append(std::span<const std::uint8_t> data);

        void SetEndOfStream();

        void Read(const std::int64_t& pos, std::span<std::uint8_t> data) override;

        [[nodiscard]] std::int64_t GetLength() const override;

        [[nodiscard]] std::int64_t GetAvailable() const override;

        void MakeAvailable(const std::uint64_t& size) override;

    private:
        std::vector<std::uint8_t> _buffer;
        bool _eos{false};
    };
}
