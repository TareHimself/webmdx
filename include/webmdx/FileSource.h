#pragma once
#include <filesystem>
#include <fstream>
#include "ISource.h"

namespace wdx {
    class FileSource final : public ISource {
    public:
        explicit FileSource(const std::filesystem::path &path);

        ~FileSource() override;

        void Read(std::size_t pos, std::size_t size, unsigned char *data) override;

        [[nodiscard]] std::size_t GetLength() const override;

        [[nodiscard]] std::size_t GetAvailable() const override;

        [[nodiscard]] bool IsWriting() const override;

    private:
        std::filesystem::path _path;
        size_t _fileSize;
        std::ifstream _fileStream;
        size_t _pos;
    };
}
