#pragma once

#include <cstddef>
#include <cstdint>

namespace Utils
{

    enum class OpenMode
    {
        Create,
        CreateTruncate,
        ReadWrite,
        ReadWriteSync,
        ReadOnly,
        WriteOnly,
    };

    enum class BufferMode
    {
        Buffered,
        Direct,
    };

    enum SeekOrigin
    {
        Begin,
        Current,
        End,
    };

    class IFileHandle
    {
    public:
        IFileHandle(const IFileHandle &) = delete;
        IFileHandle &operator=(const IFileHandle &) = delete;

        virtual ~IFileHandle() = default;
        virtual std::size_t Read(void *bufferPtr, std::size_t size) = 0;
        virtual std::size_t Write(const void *bufferPtr, std::size_t size) = 0;
        virtual uint64_t Seek(SeekOrigin origin, int64_t offset) = 0;

    protected:
        IFileHandle() = default;
    };

    class IFileMapping
    {
    public:
        // Enum for access types
        enum class Access
        {
            Read,
            Write,
            ReadWrite
        };

        virtual ~IFileMapping() = default;

        // Pure virtual functions that need to be implemented by derived classes
        virtual void *Ptr() = 0;
        virtual const void *Ptr() const = 0;

        // Prevent copying
        IFileMapping(const IFileMapping &) = delete;
        IFileMapping &operator=(const IFileMapping &) = delete;

    protected:
        IFileMapping() = default; // Protected constructor to prevent direct instantiation
    };

} // namespace Embedded