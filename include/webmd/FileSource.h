#pragma once
#include <filesystem>
#include <fstream>
#include "ISource.h"

namespace wd {
    class FileSource final : public ISource {
    public:
        explicit FileSource(const std::filesystem::path &path);

        ~FileSource() override;

        void Read(size_t pos, size_t size, unsigned char *data) override;

        [[nodiscard]] size_t GetLength() const override;

        [[nodiscard]] size_t GetAvailable() const override;

        [[nodiscard]] bool IsWriting() const override;

    private:
        std::filesystem::path _path;
        size_t _fileSize;
        std::ifstream _fileStream;
        size_t _pos;
    };
}
