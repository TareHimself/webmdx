#pragma once
#include <filesystem>
#include <fstream>
#include "ISource.h"
#include "export.h"
namespace wdx {
    class WEBMDX_API FileSource final : public ISource {
    public:
        explicit FileSource(const std::filesystem::path &path);

        ~FileSource() override;

        void Read(const std::int64_t& pos, std::span<std::uint8_t> data) override;

        [[nodiscard]] std::int64_t GetLength() const override;

        [[nodiscard]] std::int64_t GetAvailable() const override;
        void MakeAvailable(const std::uint64_t& size) override;

    private:
        std::filesystem::path _path;
        std::int64_t _fileSize;
        std::ifstream _fileStream;
        std::int64_t _pos;
    };
}
