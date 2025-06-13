#pragma once
#include <vector>

namespace wd {
    class ISource {
    public:
        virtual ~ISource() = default;

        virtual void Read(size_t pos,size_t size,unsigned char * data) = 0;
        [[nodiscard]] virtual size_t GetLength() const = 0;
        [[nodiscard]] virtual size_t GetAvailable() const = 0;
        [[nodiscard]] virtual bool IsWriting() const = 0;
        [[nodiscard]] bool IsEmpty() const;
    };
}
