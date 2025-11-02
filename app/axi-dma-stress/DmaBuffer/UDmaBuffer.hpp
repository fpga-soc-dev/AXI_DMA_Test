
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

    class UDmaBuffer
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

        enum class SyncDirection : uint32_t
        {
            DMA_BIDIRECTIONAL = 0,
            DMA_TO_DEVICE = 1,
            DMA_FROM_DEVICE = 2,
        };

        UDmaBuffer(uint8_t buffer_number, uint32_t size, CachePolicy cache_policy, uint8_t quirk_mmap_mode = 2);
        ~UDmaBuffer();

        bool CreateBuffer(uint8_t buffer_number, uint32_t size, CachePolicy cache_policy, uint8_t quirk_mmap_mode = 2);

        DriverInfo GetDriverInfo(std::string buffer_name) const;
        DeviceInfo GetDeviceInfo(std::string buffer_name) const;
        SyncInfo GetSyncInfo(std::string buffer_name) const;
        void SetSyncInfo(std::string buffer_name, uint64_t offset, uint64_t size, SyncDirection sync_direction);
        std::size_t GetBufferSize(std::string buffer_name) const;
        uint64_t GetPhysicalAddress(std::string buffer_name) const;
        std::vector<std::string> GetAllocatedBuffers() const;

        // This ioctl is for get owner of the memory block in the manual cache management mode.
        // If this value is 1, the buffer is owned by the device. If this value is 0,
        // the buffer is owned by the cpu.
        int GetSyncOwner(std::string buffer_name) const;

        std::byte *BufferPtr(std::string buffer_name) const;
        std::size_t Offset(std::string buffer_name) const;

        // This ioctl writes a value to sync_for_cpu. If '1' is written to sync_for_cpu,
        // if sync_direction is 2(=DMA_FROM_DEVICE) or 0(=DMA_BIDIRECTIONAL),
        // the write to the device file invalidates a cache specified by sync_offset and sync_size.
        void InvalidateCache(std::string buffer_name);
        void InvalidateCache(std::string buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction);

        // This ioctl writes a value to sync_for_device.
        // If '1' is written to sync_for_device, if sync_direction is 1(=DMA_TO_DEVICE) or 0(=DMA_BIDIRECTIONAL),
        // the write to the device file flushes a cache specified by sync_offset and sync_size
        // (i.e. the cached data, if any, will be updated with data on DDR memory).
        void FlushCache(std::string buffer_name);
        void FlushCache(std::string buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction);

        void PrintBuffersInfo() const;

    private:
        const std::string m_BufferName;
        const std::string m_Devicepath;
        const std::string m_Classpath;
        const CachePolicy m_Policy;

        std::unique_ptr<UDmaBufferAllocate> m_BufferAllocator;

        std::unordered_map<std::string, int> m_BufferFds;
        std::unordered_map<std::string, std::byte *> m_BufferPtr;
        std::unordered_map<std::string, DriverInfo> m_DriverInfos;
        std::unordered_map<std::string, DeviceInfo> m_DeviceInfos;
        std::unordered_map<std::string, SyncInfo> m_SyncInfo;

        void PrintDriverInfo(const std::string buffer_name) const;
        void PrintDeviceInfo(const std::string buffer_name) const;
        void PrintSyncInfo(const std::string buffer_name) const;
    };

} // namespace Dma
