#include "FileHandle.hpp"
#include <stdexcept>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

namespace Utils
{

    FileHandle::FileHandle(const char *filename, OpenMode openMode, BufferMode bufferMode) : _fd(-1)
    {
        int flags{0};
        if (bufferMode == BufferMode::Direct)
        {
            flags |= O_DIRECT;
        }

        switch (openMode)
        {
        case OpenMode::Create:
            flags |= O_CREAT | O_EXCL | O_RDWR;
            break;
        case OpenMode::CreateTruncate:
            flags |= O_CREAT | O_TRUNC | O_RDWR;
            break;
        case OpenMode::ReadWrite:
            flags |= O_RDWR;
            break;
        case OpenMode::ReadWriteSync:
            flags |= O_SYNC | O_RDWR;
            break;
        case OpenMode::ReadOnly:
            flags |= O_RDONLY;
            break;
        case OpenMode::WriteOnly:
            flags |= O_WRONLY;
            break;
        default:
            throw std::runtime_error("Invalid open mode");
        }
        _fd = open64(filename, flags, S_IRUSR | S_IWUSR);
        if (_fd == -1)
        {
            throw std::runtime_error(std::string("Failed to open file: ") + strerror(errno));
        }
    }

    FileHandle::FileHandle(const std::string &filename, OpenMode openMode, BufferMode bufferMode) : FileHandle(filename.c_str(), openMode, bufferMode)
    {
    }

    FileHandle::FileHandle(const char *filename, OpenMode openMode) : FileHandle(filename, openMode, BufferMode::Buffered)
    {
    }

    FileHandle::FileHandle(const std::string &filename, OpenMode openMode) : FileHandle(filename.c_str(), openMode, BufferMode::Buffered)
    {
    }

    FileHandle::~FileHandle()
    {
        if (_fd != -1)
        {
            close(_fd);
        }
    }

    std::size_t FileHandle::Read(void *bufferPtr, std::size_t size)
    {
        auto read_size{read(_fd, bufferPtr, size)};
        if (read_size < 0)
        {
            throw std::runtime_error(std::string("Failed to read file: ") + strerror(errno));
        }
        return read_size;
    }

    std::size_t FileHandle::Write(const void *bufferPtr, std::size_t size)
    {
        auto write_size{write(_fd, bufferPtr, size)};
        if (write_size < 0)
        {
            throw std::runtime_error(std::string("Failed to write file: ") + strerror(errno));
        }
        return write_size;
    }

    uint64_t FileHandle::Seek(SeekOrigin origin, int64_t offset)
    {
        int64_t position{0};
        switch (origin)
        {
        case SeekOrigin::Begin:
            position = SEEK_SET;
            break;
        case SeekOrigin::Current:
            position = SEEK_CUR;
            break;
        case SeekOrigin::End:
            position = SEEK_END;
            break;
        default:
            throw std::runtime_error("Invalid seek origin");
        }
        auto new_offset{lseek64(_fd, offset, position)};
        if (new_offset < 0)
        {
            throw std::runtime_error(std::string("Failed to seek file: ") + strerror(errno));
        }
        return new_offset;
    }

    int FileHandle::Handle() const
    {
        return _fd;
    }

} // namespace Embedded