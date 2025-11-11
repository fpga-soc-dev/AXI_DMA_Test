#include "UDmaBuffer.hpp"
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <spdlog/spdlog.h>

namespace Dma
{

    UDmaBuffer::UDmaBuffer(CachePolicy cache_policy) : m_Policy(cache_policy), m_BufferAllocator(std::make_unique<UDmaBufferAllocate>())
    {
    }

    UDmaBuffer::~UDmaBuffer()
    {
        // unmap
        for (auto &kv : m_Buffers)
        {
            auto &entry = kv.second;
            if (entry.ptr != nullptr && entry.size > 0)
            {
                ::munmap(entry.ptr, entry.size);
                entry.ptr = nullptr;
            }
        }

        // then free allocated buffers
        for (auto &kv : m_Buffers)
        {
            auto &entry = kv.second;
            if (entry.fd >= 0)
            {
                ::close(entry.fd);
                entry.fd = -1;
            }
        }
    }

    bool UDmaBuffer::CreateBuffer(const UDmaBufferAllocate::UdmaBufConfig &cfg, uint8_t quirk_mmap_mode)
    {

        auto allocated_buffers = m_BufferAllocator->GetAllocatedBuffers();
        if (allocated_buffers.size() == 0)
        {
            if (!m_BufferAllocator->AllocateBuffers(cfg, quirk_mmap_mode))
            {
                SPDLOG_ERROR("Failed to allocate DMA buffers");
                return false;
            }
            allocated_buffers = m_BufferAllocator->GetAllocatedBuffers();
        }
        else
        {
            for (const auto &buf_name : allocated_buffers)
            {
                SPDLOG_INFO("Found already allocated buffer: {}", buf_name);
            }
        }

        for (const auto &buf_name : allocated_buffers)
        {
            BufferEntry entry{};
            const std::string dev_path = "/dev/" + buf_name;
            int open_flags = O_RDWR;
            if (m_Policy == CachePolicy::NonCached)
            {
                open_flags |= O_SYNC;
            }

            entry.fd = ::open(dev_path.c_str(), open_flags);
            if (entry.fd < 0)
            {
                SPDLOG_ERROR("UDmaBuffer: failed to open {}: {}", dev_path, strerror(errno));
                return false;
            }

            // get size
            if (ioctl(entry.fd, U_DMA_BUF_IOCTL_GET_SIZE, &entry.size) < 0 || entry.size == 0)
            {
                SPDLOG_ERROR("UDmaBuffer: failed to get size for {}: {}", dev_path, strerror(errno));
                ::close(entry.fd);
                return false;
            }

            // get physical address
            if (ioctl(entry.fd, U_DMA_BUF_IOCTL_GET_DMA_ADDR, &entry.phys_addr) < 0)
            {
                SPDLOG_ERROR("UDmaBuffer: failed to get physical address for {}: {}", dev_path, strerror(errno));
                ::close(entry.fd);
                return false;
            }

            // get driver info
            {
                u_dma_buf_ioctl_drv_info drv{};
                if (::ioctl(entry.fd, U_DMA_BUF_IOCTL_GET_DRV_INFO, &drv) == 0)
                {
                    entry.driver = {
                        GET_U_DMA_BUF_IOCTL_FLAGS_IOCTL_VERSION(&drv),
                        GET_U_DMA_BUF_IOCTL_FLAGS_IN_KERNEL_FUNCTIONS(&drv),
                        GET_U_DMA_BUF_IOCTL_FLAGS_USE_OF_DMA_CONFIG(&drv),
                        GET_U_DMA_BUF_IOCTL_FLAGS_USE_OF_RESERVED_MEM(&drv),
                        GET_U_DMA_BUF_IOCTL_FLAGS_USE_QUIRK_MMAP(&drv),
                        GET_U_DMA_BUF_IOCTL_FLAGS_USE_QUIRK_MMAP_PAGE(&drv),
                        std::string(drv.version)};
                }
                else
                {
                    SPDLOG_WARN("UDmaBuffer: failed to get driver info for {}: {}", buf_name, strerror(errno));
                }
            }

            // device info
            {
                u_dma_buf_ioctl_dev_info dev{};
                if (::ioctl(entry.fd, U_DMA_BUF_IOCTL_GET_DEV_INFO, &dev) == 0)
                {
                    entry.device = {
                        GET_U_DMA_BUF_IOCTL_FLAGS_DMA_MASK(&dev),
                        GET_U_DMA_BUF_IOCTL_FLAGS_DMA_COHERENT(&dev),
                        GET_U_DMA_BUF_IOCTL_FLAGS_MMAP_MODE(&dev)};
                }
                else
                {
                    SPDLOG_WARN("UDmaBuffer: failed to get device info for {}: {}", buf_name, strerror(errno));
                }
            }

            // sync info
            {
                u_dma_buf_ioctl_sync_args sa{};
                if (::ioctl(entry.fd, U_DMA_BUF_IOCTL_GET_SYNC, &sa) == 0)
                {
                    entry.sync = {
                        sa.offset,
                        sa.size,
                        GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&sa),
                        GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_OWNER(&sa),
                        GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_MODE(&sa),
                    };
                }
                else
                {
                    entry.sync = {};
                }
            }

            // mmap
            void *p = ::mmap(nullptr,
                             entry.size,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             entry.fd,
                             0);
            if (p == MAP_FAILED)
            {
                SPDLOG_ERROR("UDmaBuffer: mmap failed for {}: {}", buf_name, strerror(errno));
                ::close(entry.fd);
                return false;
            }
            entry.ptr = static_cast<std::byte *>(p);

            m_Buffers[buf_name] = entry;
        }

        return true;
    }

    std::vector<std::string> UDmaBuffer::GetAllocatedBuffers() const
    {
        std::vector<std::string> out;
        out.reserve(m_Buffers.size());
        for (const auto &kv : m_Buffers)
        {
            out.push_back(kv.first);
        }
        return out;
    }

    std::byte *UDmaBuffer::BufferPtr(std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("BufferPtr: invalid buffer name: {}", buffer_name);
            return nullptr;
        }
        return it->second.ptr;
    }

    void UDmaBuffer::InvalidateCache(std::string buffer_name)
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("InvalidateCache: invalid buffer name: {}", buffer_name);
            return;
        }

        uint64_t sync_for_cpu{1};
        if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_SET_SYNC_FOR_CPU, &sync_for_cpu) < 0)
        {
            SPDLOG_ERROR("Failed to invalidate cache for {}", buffer_name);
        }
    }

    void UDmaBuffer::InvalidateCache(std::string buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction)
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("InvalidateCache(offset): invalid buffer name: {}", buffer_name);
            return;
        }

        std::size_t sync_offset = offset;
        std::size_t sync_size = size;
        uint32_t sync_dir = static_cast<uint32_t>(sync_direction);
        uint64_t sync_for_cpu = ((uint64_t)(sync_offset & 0xFFFFFFFF) << 32) |
                                ((uint64_t)(sync_size & 0xFFFFFFF0) << 0) |
                                ((uint64_t)(sync_dir & 0x00000003) << 2) |
                                0x00000001;
        if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_SET_SYNC_FOR_CPU, &sync_for_cpu) < 0)
        {
            SPDLOG_ERROR("Failed to invalidate cache for {}", buffer_name);
        }
    }

    void UDmaBuffer::FlushCache(std::string buffer_name)
    {
        if (m_Policy == CachePolicy::Cached)
        {
            auto it = m_Buffers.find(buffer_name);
            if (it == m_Buffers.end())
            {
                SPDLOG_ERROR("FlushCache: invalid buffer name: {}", buffer_name);
                return;
            }

            uint64_t sync_for_device = 1;
            if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_SET_SYNC_FOR_DEVICE, &sync_for_device) < 0)
            {
                SPDLOG_ERROR("Failed to flush cache for {}", buffer_name);
            }
        }
    }

    void UDmaBuffer::FlushCache(std::string buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction)
    {
        if (m_Policy == CachePolicy::NonCached)
            return;

        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("FlushCache(offset): invalid buffer name: {}", buffer_name);
            return;
        }

        std::size_t sync_offset = offset;
        std::size_t sync_size = size;
        uint32_t sync_dir = static_cast<uint32_t>(sync_direction);
        uint64_t sync_for_device = ((uint64_t)(sync_offset & 0xFFFFFFFF) << 32) |
                                   ((uint64_t)(sync_size & 0xFFFFFFF0) << 0) |
                                   ((uint64_t)(sync_dir & 0x00000003) << 2) |
                                   0x00000001;
        if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_SET_SYNC_FOR_DEVICE, &sync_for_device) < 0)
        {
            SPDLOG_ERROR("Failed to flush cache for {}", buffer_name);
        }
    }

    int UDmaBuffer::GetSyncOwner(std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("GetSyncOwner: invalid buffer name: {}", buffer_name);
            return -1;
        }

        int owner{0};
        if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_GET_SYNC_OWNER, &owner) < 0)
        {
            SPDLOG_ERROR("Failed to get sync owner from {}", buffer_name);
            return -1;
        }
        return owner;
    }

    uint64_t UDmaBuffer::GetPhysicalAddress(std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("GetPhysicalAddress: invalid buffer name: {}", buffer_name);
            return 0;
        }
        uint64_t phys_addr{0};
        if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_GET_DMA_ADDR, &phys_addr) < 0)
        {
            SPDLOG_ERROR("Failed to get physical address from {}", buffer_name);
            return 0;
        }
        return phys_addr;
    }

    std::size_t UDmaBuffer::GetBufferSize(std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("GetBufferSize: invalid buffer name: {}", buffer_name);
            return 0;
        }

        std::size_t buf_size{0};
        if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_GET_SIZE, &buf_size) < 0 || buf_size == 0)
        {
            SPDLOG_ERROR("Failed to get buffer size from {}", buffer_name);
            return 0;
        }

        return buf_size;
    }

    void UDmaBuffer::SetSyncInfo(std::string buffer_name, uint64_t offset, uint64_t size, SyncDirection sync_direction)
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("SetSyncInfo: invalid buffer name: {}", buffer_name);
            return;
        }

        u_dma_buf_ioctl_sync_args sync_args = {0};
        sync_args.offset = offset;
        sync_args.size = size;
        auto sync_dir = static_cast<uint32_t>(sync_direction);

        SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&sync_args, sync_dir);

        if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_SET_SYNC, &sync_args) < 0)
        {
            SPDLOG_ERROR("Failed to set sync info for {}", buffer_name);
            return;
        }

        // update cached copy
        it->second.sync = {
            sync_args.offset,
            sync_args.size,
            GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&sync_args),
            GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_OWNER(&sync_args),
            GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_MODE(&sync_args)};
    }

    UDmaBuffer::SyncInfo UDmaBuffer::GetSyncInfo(std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("GetSyncInfo: invalid buffer name: {}", buffer_name);
            return {};
        }

        // This ioctl is for get owner of the memory block in the manual cache management mode. If this value is 1, the buffer is owned by the device.
        // If this value is 0, the buffer is owned by the cpu.
        u_dma_buf_ioctl_sync_args sync_args = {0};
        if (::ioctl(it->second.fd, U_DMA_BUF_IOCTL_GET_SYNC, &sync_args) < 0)
        {
            SPDLOG_ERROR("Failed to get sync info from {}", buffer_name);
            return {};
        }

        return {
            sync_args.offset,
            sync_args.size,
            GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&sync_args),
            GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_OWNER(&sync_args),
            GET_U_DMA_BUF_IOCTL_FLAGS_SYNC_MODE(&sync_args)};
    }

    UDmaBuffer::DeviceInfo UDmaBuffer::GetDeviceInfo(std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("GetDeviceInfo: invalid buffer name: {}", buffer_name);
            return {};
        }

        u_dma_buf_ioctl_dev_info dev_info{0};

        if (ioctl(it->second.fd, U_DMA_BUF_IOCTL_GET_DEV_INFO, &dev_info) < 0)
        {
            SPDLOG_ERROR("Failed to get device info from {}", buffer_name);
            return {};
        }

        return {
            GET_U_DMA_BUF_IOCTL_FLAGS_DMA_MASK(&dev_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_DMA_COHERENT(&dev_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_MMAP_MODE(&dev_info)};
    }

    UDmaBuffer::DriverInfo UDmaBuffer::GetDriverInfo(std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("GetDriverInfo: invalid buffer name: {}", buffer_name);
            return {};
        }

        u_dma_buf_ioctl_drv_info drv_info{0};

        if (ioctl(it->second.fd, U_DMA_BUF_IOCTL_GET_DRV_INFO, &drv_info) < 0)
        {
            SPDLOG_ERROR("Failed to get driver info from {}", buffer_name);
            return {};
        }

        return {
            GET_U_DMA_BUF_IOCTL_FLAGS_IOCTL_VERSION(&drv_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_IN_KERNEL_FUNCTIONS(&drv_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_USE_OF_DMA_CONFIG(&drv_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_USE_OF_RESERVED_MEM(&drv_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_USE_QUIRK_MMAP(&drv_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_USE_QUIRK_MMAP_PAGE(&drv_info),
            std::string(drv_info.version)};
    }

    void UDmaBuffer::PrintDriverInfo(const std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("PrintDriverInfo: invalid buffer name: {}", buffer_name);
            return;
        }

        const auto &info = it->second.driver;
        SPDLOG_DEBUG("Driver Info for Buffer {}:", buffer_name);
        SPDLOG_DEBUG("  Version: {}", info.Version);
        SPDLOG_DEBUG("  IOC Version: {}", info.IocVersion);
        SPDLOG_DEBUG("  In Kernel Functions: {}", info.InKernelFunctions);
        SPDLOG_DEBUG("  Use of DMA Config: {}", info.UseOfDmaConfig);
        SPDLOG_DEBUG("  Use of Reserved Mem: {}", info.UseOfReservedMem);
        SPDLOG_DEBUG("  Use Quirk Mmap: {}", info.UseQuirkMmap);
        SPDLOG_DEBUG("  Use Quirk Mmap Page: {}", info.UseQuirkMmapPage);
    }

    void UDmaBuffer::PrintDeviceInfo(const std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("PrintDeviceInfo: invalid buffer name: {}", buffer_name);
            return;
        }

        const auto &info = it->second.device;
        SPDLOG_DEBUG("Device Info for Buffer {}:", buffer_name);
        SPDLOG_DEBUG("  DMA Mask: {}", info.DmaMask);
        SPDLOG_DEBUG("  DMA Coherent: {}", info.DmaCoherent);
        SPDLOG_DEBUG("  Mmap Mode: {}", info.MmapMode);
    }

    void UDmaBuffer::PrintSyncInfo(const std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("PrintSyncInfo: invalid buffer name: {}", buffer_name);
            return;
        }
        const auto &info = it->second.sync;

        SPDLOG_DEBUG("Sync Info for Buffer {}:", buffer_name);
        SPDLOG_DEBUG("  Sync Offset: {}", info.SyncOffset);
        SPDLOG_DEBUG("  Sync Size: {}", info.SyncSize);
        SPDLOG_DEBUG("  Sync Direction: {}", info.SyncDir);
        SPDLOG_DEBUG("  Sync Owner: {}", info.SyncOwner);
        SPDLOG_DEBUG("  Sync Mode: {}", info.SyncMode);
    }

    std::size_t UDmaBuffer::Offset(const std::string buffer_name) const
    {
        auto it = m_Buffers.find(buffer_name);
        if (it == m_Buffers.end())
        {
            SPDLOG_ERROR("Offset: invalid buffer name: {}", buffer_name);
            return 0;
        }
        return it->second.sync.SyncOffset;
    }

    void UDmaBuffer::PrintBuffersInfo() const
    {
        for (const auto &kv : m_Buffers)
        {
            const auto &name = kv.first;
            SPDLOG_DEBUG("=== DMA Buffer {} Info ===", name);
            PrintDriverInfo(name);
            PrintDeviceInfo(name);
            PrintSyncInfo(name);
            SPDLOG_DEBUG("=========================");
        }
    }

} // namespace Dma
