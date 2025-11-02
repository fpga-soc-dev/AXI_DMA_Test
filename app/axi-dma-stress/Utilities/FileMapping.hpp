#pragma once

#include "FileHandle.hpp"

namespace Utils
{
    class FileMapping
    {
    public:
        enum class Access
        {
            Read,
            Write,
            ReadWrite
        };
        FileMapping(const FileMapping &) = delete;
        FileMapping &operator=(const FileMapping &) = delete;

        FileMapping(const FileHandle &fileHandle, off_t offset, std::size_t size, Access access);
        FileMapping(int fd, std::size_t length, off_t offset, Access access);
        ~FileMapping();

        void *Ptr();
        const void *Ptr() const;

    private:
        void *_ptr;
        std::size_t _length;
    };
} // namespace Embedded