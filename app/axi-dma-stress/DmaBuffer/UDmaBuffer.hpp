
#pragma once

#include "UDmaBufferAllocate.hpp"
#include "UDmaBufferIoctl.hpp"
#include <string>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Dma
{
    class ICacheManager
    {
    public:
        enum class SyncDirection : uint32_t
        {
            DMA_BIDIRECTIONAL = 0,
            DMA_TO_DEVICE = 1,
            DMA_FROM_DEVICE = 2,
        };

        virtual ~ICacheManager() = default;
        virtual void FlushCache(const std::string &buffer_name) = 0;
        virtual void FlushCache(const std::string &buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction) = 0;
        virtual void InvalidateCache(const std::string &buffer_name) = 0;
        virtual void InvalidateCache(const std::string &buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction) = 0;
        virtual std::uint64_t GetPhysicalAddress(const std::string &buffer_name) const = 0;
        virtual std::size_t GetBufferSize(const std::string &buffer_name) const = 0;
        virtual std::byte *BufferPtr(const std::string &buffer_name) const = 0;
    };

    class UDmaBuffer : public ICacheManager
    {
    public:
        struct DriverInfo
        {
            int IocVersion{0};
            int InKernelFunctions{0};
            int UseOfDmaConfig{0};
            int UseOfReservedMem{0};
            int UseQuirkMmap{0};
            int UseQuirkMmapPage{0};
            std::string Version;
        };

        struct DeviceInfo
        {
            int DmaMask{0};
            int DmaCoherent{0};
            int MmapMode{0};
        };

        struct SyncInfo
        {
            uint64_t SyncOffset{0};
            uint64_t SyncSize{0};
            int SyncDir{0};
            int SyncOwner{0};
            int SyncMode{0};
        };

        enum class CachePolicy
        {
            NonCached,
            Cached,
        };

        struct BufferEntry
        {
            int fd{-1};
            std::byte *ptr{nullptr};
            std::uint64_t phys_addr{0};
            std::size_t size{0};
            DriverInfo driver{};
            DeviceInfo device{};
            SyncInfo sync{};
        };

        explicit UDmaBuffer(CachePolicy cache_policy);
        ~UDmaBuffer();

        UDmaBuffer(UDmaBuffer &&other) noexcept;
        UDmaBuffer &operator=(UDmaBuffer &&other) noexcept;
        UDmaBuffer(const UDmaBuffer &) = delete;
        UDmaBuffer &operator=(const UDmaBuffer &) = delete;

        bool CreateBuffer(const UDmaBufferAllocate::UdmaBufConfig &cfg, uint8_t quirk_mmap_mode = 2);
        void DeAllocateBuffers();
        bool IsBufferAllocated(const std::string &buffer_name) const;

        DriverInfo GetDriverInfo(const std::string &buffer_name) const;
        DeviceInfo GetDeviceInfo(const std::string &buffer_name) const;
        SyncInfo GetSyncInfo(const std::string &buffer_name) const;
        void SetSyncInfo(const std::string &buffer_name, uint64_t offset, uint64_t size, SyncDirection sync_direction);
        std::vector<std::string> GetAllocatedBuffers() const;

        // This ioctl is for get owner of the memory block in the manual cache management mode.
        // If this value is 1, the buffer is owned by the device. If this value is 0,
        // the buffer is owned by the cpu.
        int GetSyncOwner(const std::string &buffer_name) const;

        std::size_t Offset(const std::string &buffer_name) const;

        std::size_t GetBufferSize(const std::string &buffer_name) const override;
        std::uint64_t GetPhysicalAddress(const std::string &buffer_name) const override;
        std::byte *BufferPtr(const std::string &buffer_name) const override;

        // This ioctl writes a value to sync_for_cpu. If '1' is written to sync_for_cpu,
        // if sync_direction is 2(=DMA_FROM_DEVICE) or 0(=DMA_BIDIRECTIONAL),
        // the write to the device file invalidates a cache specified by sync_offset and sync_size.
        void InvalidateCache(const std::string &buffer_name) override;
        void InvalidateCache(const std::string &buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction) override;

        // This ioctl writes a value to sync_for_device.
        // If '1' is written to sync_for_device, if sync_direction is 1(=DMA_TO_DEVICE) or 0(=DMA_BIDIRECTIONAL),
        // the write to the device file flushes a cache specified by sync_offset and sync_size
        // (i.e. the cached data, if any, will be updated with data on DDR memory).
        void FlushCache(const std::string &buffer_name) override;
        void FlushCache(const std::string &buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction) override;

        void PrintBuffersInfo() const;

    private:
        CachePolicy m_Policy;
        std::vector<std::string> m_AllocatedBufferNames;
        std::unordered_map<std::string, BufferEntry> m_Buffers;

        void PrintDriverInfo(const std::string &buffer_name) const;
        void PrintDeviceInfo(const std::string &buffer_name) const;
        void PrintSyncInfo(const std::string &buffer_name) const;

        bool OpenAndMapBuffer(const std::string &buffer_name);
        void UnmapAndCloseBuffer(const std::string &buffer_name);
    };

} // namespace Dma
