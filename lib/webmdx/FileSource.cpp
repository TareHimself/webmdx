#include "webmdx/FileSource.h"

namespace wdx {
    FileSource::FileSource(const std::filesystem::path &path): _pos(0) {
        _path = path;
        _fileSize = std::filesystem::file_size(_path);
        _fileStream = std::ifstream(_path, std::ios::binary);
    }

    FileSource::~FileSource() {
        _fileStream.close();
    }

    void FileSource::Read(const std::int64_t& pos, std::span<std::uint8_t> data) {
        if (_pos != pos) {
            _fileStream.seekg(pos);
            _pos = pos;
        }
        _fileStream.read(reinterpret_cast<char *>(data.data()), static_cast<std::int64_t>(data.size()));
        _pos += static_cast<std::int64_t>(data.size());
    }

    std::int64_t FileSource::GetLength() const {
        return _fileSize;
    }

    std::int64_t FileSource::GetAvailable() const {
        return _fileSize;
    }

    void FileSource::MakeAvailable(const std::uint64_t& size)
    {
        // No need , whole file is already available
    }
}
