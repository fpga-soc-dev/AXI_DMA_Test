#pragma once

#include "Types.hpp"
#include <string>

namespace Utils
{
    class FileHandle : public IFileHandle
    {
    public:
        FileHandle(const FileHandle &) = delete;
        FileHandle &operator=(const FileHandle &) = delete;

        FileHandle(const std::string &filename, OpenMode openMode, BufferMode bufferMode);
        FileHandle(const char *filename, OpenMode openMode, BufferMode bufferMode);
        FileHandle(const std::string &filename, OpenMode openMode);
        FileHandle(const char *filename, OpenMode openMode);
        ~FileHandle();

        virtual std::size_t Read(void *bufferPtr, std::size_t size) override;
        virtual std::size_t Write(const void *bufferPtr, std::size_t size) override;
        virtual uint64_t Seek(SeekOrigin origin, int64_t offset) override;

        int Handle() const;

    private:
        int _fd;
    };
} // namespace Utils