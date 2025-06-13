#include "webmd/FileSource.h"
namespace wd {
    FileSource::FileSource(const std::filesystem::path &path) {
        _path = path;
        _fileSize = std::filesystem::file_size(_path);
        _fileStream = std::ifstream(_path, std::ios::binary);
    }

    FileSource::~FileSource() {
        _fileStream.close();
    }

    void FileSource::Read(size_t pos, size_t size, unsigned char *data) {
        if (_pos != pos) {
            _fileStream.seekg(pos);
            _pos = pos;
        }
        _fileStream.read(reinterpret_cast<char *>(data), size);
        _pos += size;
    }

    size_t FileSource::GetLength() const {
        return _fileSize;
    }

    size_t FileSource::GetAvailable() const {
        return _fileSize;
    }

    bool FileSource::IsWriting() const {
        return false;
    }
}
