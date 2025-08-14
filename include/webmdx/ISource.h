#pragma once
#include <cstddef>
#include "export.h"
namespace wdx {
    class WEBMDX_API ISource {
    public:
        virtual ~ISource() = default;

        virtual void Read(std::size_t pos, std::size_t size, unsigned char *data) = 0;

        [[nodiscard]] virtual std::size_t GetLength() const = 0;

        [[nodiscard]] virtual std::size_t GetAvailable() const = 0;

        [[nodiscard]] virtual bool IsWriting() const = 0;

        [[nodiscard]] bool IsEmpty() const;
    };
}
