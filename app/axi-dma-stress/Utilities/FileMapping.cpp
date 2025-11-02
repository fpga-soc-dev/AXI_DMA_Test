#include "FileMapping.hpp"
#include <sys/mman.h>
#include <stdexcept>

namespace Utils
{
    FileMapping::FileMapping(const FileHandle &fileHandle, off_t offset, std::size_t size, Access access)
        : _ptr(nullptr), _length(size)
    {
        int prot = 0;
        switch (access)
        {
        case Access::Read:
            prot = PROT_READ;
            break;
        case Access::Write:
            prot = PROT_WRITE;
            break;
        case Access::ReadWrite:
            prot = PROT_READ | PROT_WRITE;
            break;
        default:
            break;
        }

        _ptr = mmap(nullptr, size, prot, MAP_SHARED, fileHandle.Handle(), offset);
    }

    FileMapping::FileMapping(int fd, std::size_t length, off_t offset, Access access)
        : _ptr(nullptr), _length(length)
    {
        int prot = 0;
        switch (access)
        {
        case Access::Read:
            prot = PROT_READ;
            break;
        case Access::Write:
            prot = PROT_WRITE;
            break;
        case Access::ReadWrite:
            prot = PROT_READ | PROT_WRITE;
            break;
        default:
            throw std::runtime_error("Invalid access mode");
            break;
        }

        _ptr = mmap(nullptr, length, prot, MAP_SHARED, fd, offset);
        if (_ptr == MAP_FAILED)
        {
            throw std::runtime_error("mmap failed");
        }
    }

    FileMapping::~FileMapping()
    {
        if (_ptr != nullptr)
        {
            munmap(_ptr, _length);
        }
    }

    void *FileMapping::Ptr()
    {
        return _ptr;
    }

    const void *FileMapping::Ptr() const
    {
        return _ptr;
    }
} // namespace Embedded