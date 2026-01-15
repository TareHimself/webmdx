#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "export.h"
namespace wdx {
    class WEBMDX_API ISource {
    public:
        virtual ~ISource() = default;

        virtual void Read(const std::int64_t& pos, std::span<std::uint8_t> data) = 0;

        [[nodiscard]] virtual std::int64_t GetLength() const = 0;

        [[nodiscard]] virtual std::int64_t GetAvailable() const = 0;

        virtual void MakeAvailable(const std::uint64_t& size) = 0;
    };
}
