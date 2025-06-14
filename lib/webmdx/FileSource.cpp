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

    void FileSource::Read(std::size_t pos, std::size_t size, unsigned char *data) {
        if (_pos != pos) {
            _fileStream.seekg(pos);
            _pos = pos;
        }
        _fileStream.read(reinterpret_cast<char *>(data), size);
        _pos += size;
    }

    std::size_t FileSource::GetLength() const {
        return _fileSize;
    }

    std::size_t FileSource::GetAvailable() const {
        return _fileSize;
    }

    bool FileSource::IsWriting() const {
        return false;
    }
}
