#include <stdexcept>
#include "UDmaBuffer.hpp"
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <spdlog/spdlog.h>

namespace Dma
{

    UDmaBuffer::UDmaBuffer(uint8_t buffer_numbers, uint32_t size, CachePolicy cache_policy, uint8_t quirk_mmap_mode)
        : m_Policy(cache_policy), m_BufferAllocator(std::make_unique<UDmaBufferAllocate>())
    {

        if (!CreateBuffer(buffer_numbers, size, cache_policy, quirk_mmap_mode))
        {
            throw std::runtime_error("Failed to create DMA buffers");
        }

        for (auto &[buf_name, fd] : m_BufferFds)
        {
            int new_fd;
            if (m_Policy == CachePolicy::Cached)
            {
                new_fd = ::open(("/dev/" + buf_name).c_str(), O_RDWR);
            }
            else
            {
                new_fd = ::open(("/dev/" + buf_name).c_str(), O_RDWR | O_SYNC);
            }

            if (new_fd < 0)
            {
                throw std::runtime_error("Failed to open buffer device: " + buf_name);
            }

            fd = new_fd;
        }

        for (auto &[buf_name, fd] : m_BufferFds)
        {
            m_DriverInfos[buf_name] = GetDriverInfo(buf_name);
            m_DeviceInfos[buf_name] = GetDeviceInfo(buf_name);
            m_SyncInfo[buf_name] = GetSyncInfo(buf_name);

            auto result_ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (result_ptr == MAP_FAILED)
            {
                throw std::runtime_error("Failed to mmap buffer " + buf_name);
            }
            m_BufferPtr[buf_name] = static_cast<std::byte *>(result_ptr);
        }
    }

    UDmaBuffer::~UDmaBuffer()
    {
        // first unmap buffers
        for (auto &[buf_name, ptr] : m_BufferPtr)
        {
            if (ptr != nullptr)
            {
                munmap(ptr, GetBufferSize(buf_name));
                ptr = nullptr;
            }
        }
        // then free allocated buffers
        for (auto &[buf_name, fd] : m_BufferFds)
        {
            if (fd >= 0)
            {
                ::close(fd);
                fd = -1;
            }
        }
    }

    bool UDmaBuffer::CreateBuffer(uint8_t buffer_number, uint32_t size, CachePolicy cache_policy, uint8_t quirk_mmap_mode)
    {
        auto allocated_buffers = m_BufferAllocator->GetAllocatedBuffers();
        if (allocated_buffers.size() == 0)
        {
            if (!m_BufferAllocator->AllocateBuffers(buffer_number, size, quirk_mmap_mode))
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

        for (size_t i = 0; i < allocated_buffers.size(); ++i)
        {
            m_BufferFds[allocated_buffers[i]] = -1;
        }

        return true;
    }

    std::vector<std::string> UDmaBuffer::GetAllocatedBuffers() const
    {
        return m_BufferAllocator->GetAllocatedBuffers();
    }

    std::byte *UDmaBuffer::BufferPtr(std::string buffer_name) const
    {
        if (m_BufferFds.find(buffer_name) == m_BufferFds.end())
        {
            SPDLOG_ERROR("Invalid buffer name: {}", buffer_name);
            return nullptr;
        }

        return m_BufferPtr.at(buffer_name);
    }

    void UDmaBuffer::InvalidateCache(std::string buffer_name)
    {
        uint64_t sync_for_cpu{1};
        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_SET_SYNC_FOR_CPU, &sync_for_cpu) < 0)
        {
            SPDLOG_ERROR("Failed to invalidate cache for {}", m_Devicepath);
        }
    }

    void UDmaBuffer::InvalidateCache(std::string buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction)
    {
        std::size_t sync_offset = offset;
        std::size_t sync_size = size;
        uint32_t sync_dir = static_cast<uint32_t>(sync_direction);
        uint64_t sync_for_cpu = ((uint64_t)(sync_offset & 0xFFFFFFFF) << 32) |
                                ((uint64_t)(sync_size & 0xFFFFFFF0) << 0) |
                                ((uint64_t)(sync_dir & 0x00000003) << 2) |
                                0x00000001;
        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_SET_SYNC_FOR_CPU, &sync_for_cpu) < 0)
        {
            SPDLOG_ERROR("Failed to invalidate cache for {}", m_Devicepath);
        }
    }

    void UDmaBuffer::FlushCache(std::string buffer_name)
    {
        if (m_Policy == CachePolicy::Cached)
        {
            uint64_t sync_for_device = 1;
            if (ioctl(m_BufferFds[buffer_name], U_DMA_BUF_IOCTL_SET_SYNC_FOR_DEVICE, &sync_for_device) < 0)
            {
                SPDLOG_ERROR("Failed to flush cache for {}", m_Devicepath);
            }
        }
    }

    void UDmaBuffer::FlushCache(std::string buffer_name, std::size_t offset, std::size_t size, SyncDirection sync_direction)
    {
        std::size_t sync_offset = offset;
        std::size_t sync_size = size;
        uint32_t sync_dir = static_cast<uint32_t>(sync_direction);
        uint64_t sync_for_device = ((uint64_t)(sync_offset & 0xFFFFFFFF) << 32) |
                                   ((uint64_t)(sync_size & 0xFFFFFFF0) << 0) |
                                   ((uint64_t)(sync_dir & 0x00000003) << 2) |
                                   0x00000001;
        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_SET_SYNC_FOR_DEVICE, &sync_for_device) < 0)
        {
            SPDLOG_ERROR("Failed to flush cache for {}", m_Devicepath);
        }
    }

    int UDmaBuffer::GetSyncOwner(std::string buffer_name) const
    {
        int owner{0};
        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_GET_SYNC_OWNER, &owner) < 0)
        {
            SPDLOG_ERROR("Failed to get sync owner from {}", m_Devicepath);
            return -1;
        }
        return owner;
    }

    uint64_t UDmaBuffer::GetPhysicalAddress(std::string buffer_name) const
    {
        uint64_t phys_addr{0};
        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_GET_DMA_ADDR, &phys_addr) < 0)
        {
            SPDLOG_ERROR("Failed to get physical address from {}", m_Devicepath);
            return 0;
        }
        return phys_addr;
    }

    std::size_t UDmaBuffer::GetBufferSize(std::string buffer_name) const
    {
        std::size_t buf_size{0};
        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_GET_SIZE, &buf_size) < 0 || buf_size == 0)
        {
            SPDLOG_ERROR("Failed to get buffer size from {}", m_Devicepath);
            return 0;
        }
        return buf_size;
    }

    void UDmaBuffer::SetSyncInfo(std::string buffer_name, uint64_t offset, uint64_t size, SyncDirection sync_direction)
    {
        u_dma_buf_ioctl_sync_args sync_args = {0};
        sync_args.offset = offset;
        sync_args.size = size;
        auto sync_dir = static_cast<uint32_t>(sync_direction);

        SET_U_DMA_BUF_IOCTL_FLAGS_SYNC_DIR(&sync_args, sync_dir);

        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_SET_SYNC, &sync_args) < 0)
        {
            SPDLOG_ERROR("Failed to set sync info for {}", m_Devicepath);
        }
    }

    UDmaBuffer::SyncInfo UDmaBuffer::GetSyncInfo(std::string buffer_name) const
    {
        // This ioctl is for get owner of the memory block in the manual cache management mode. If this value is 1, the buffer is owned by the device.
        // If this value is 0, the buffer is owned by the cpu.
        u_dma_buf_ioctl_sync_args sync_args = {0};
        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_GET_SYNC, &sync_args) < 0)
        {
            SPDLOG_ERROR("Failed to get sync info from {}", m_Devicepath);
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
        u_dma_buf_ioctl_dev_info dev_info{0};

        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_GET_DEV_INFO, &dev_info) < 0)
        {
            SPDLOG_ERROR("Failed to get device info from {}", m_Devicepath);
            return {};
        }

        return {
            GET_U_DMA_BUF_IOCTL_FLAGS_DMA_MASK(&dev_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_DMA_COHERENT(&dev_info),
            GET_U_DMA_BUF_IOCTL_FLAGS_MMAP_MODE(&dev_info)};
    }

    UDmaBuffer::DriverInfo UDmaBuffer::GetDriverInfo(std::string buffer_name) const
    {
        u_dma_buf_ioctl_drv_info drv_info{0};

        if (ioctl(m_BufferFds.at(buffer_name), U_DMA_BUF_IOCTL_GET_DRV_INFO, &drv_info) < 0)
        {
            SPDLOG_ERROR("Failed to get driver info from {}", m_Devicepath);
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
        if (m_DriverInfos.find(buffer_name) == m_DriverInfos.end())
        {
            SPDLOG_ERROR("Invalid buffer name");
            return;
        }

        const auto &info = m_DriverInfos.at(buffer_name);
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
        if (m_DeviceInfos.find(buffer_name) == m_DeviceInfos.end())
        {
            SPDLOG_ERROR("Invalid buffer name");
            return;
        }

        const auto &info = m_DeviceInfos.at(buffer_name);
        SPDLOG_DEBUG("Device Info for Buffer {}:", buffer_name);
        SPDLOG_DEBUG("  DMA Mask: {}", info.DmaMask);
        SPDLOG_DEBUG("  DMA Coherent: {}", info.DmaCoherent);
        SPDLOG_DEBUG("  Mmap Mode: {}", info.MmapMode);
    }

    void UDmaBuffer::PrintSyncInfo(const std::string buffer_name) const
    {
        if (m_SyncInfo.find(buffer_name) == m_SyncInfo.end())
        {
            SPDLOG_ERROR("Invalid buffer name");
            return;
        }

        const auto &info = m_SyncInfo.at(buffer_name);
        SPDLOG_DEBUG("Sync Info for Buffer {}:", buffer_name);
        SPDLOG_DEBUG("  Sync Offset: {}", info.SyncOffset);
        SPDLOG_DEBUG("  Sync Size: {}", info.SyncSize);
        SPDLOG_DEBUG("  Sync Direction: {}", info.SyncDir);
        SPDLOG_DEBUG("  Sync Owner: {}", info.SyncOwner);
        SPDLOG_DEBUG("  Sync Mode: {}", info.SyncMode);
    }

    std::size_t UDmaBuffer::Offset(const std::string buffer_name) const
    {
        if (m_SyncInfo.find(buffer_name) == m_SyncInfo.end())
        {
            SPDLOG_ERROR("Invalid buffer name");
            return 0;
        }

        return m_SyncInfo.at(buffer_name).SyncOffset;
    }

    void UDmaBuffer::PrintBuffersInfo() const
    {
        for (const auto &[buffer_name, buffer_fd] : m_BufferFds)
        {
            SPDLOG_DEBUG("=== DMA Buffer {} Info ===", buffer_name);
            PrintDriverInfo(buffer_name);
            PrintDeviceInfo(buffer_name);
            PrintSyncInfo(buffer_name);
            SPDLOG_DEBUG("=========================");
        }
    }

} // namespace Dma
