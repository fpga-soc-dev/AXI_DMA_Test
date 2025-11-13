#pragma once

#include <cstdint>
#include <string>
#include <cstddef>
#include <vector>
#include <string_view>
#include <mutex>

namespace Dma
{
    class UDmaBufferAllocate
    {
    public:
        static constexpr std::string_view U_DMA_BUF_MODULE_NAME = "u-dma-buf.ko";
        static constexpr std::string_view U_DMA_BUF_LOADED_MODULE = "u_dma_buf";
        static constexpr std::string_view U_DMA_BUF_CLASS_PATH = "/sys/class/u-dma-buf";

        struct BufferSpec
        {
            std::uint8_t index;
            std::uint32_t size;
        };
        struct UdmaBufConfig
        {
            std::vector<BufferSpec> buffer_vec;
        };

        static UDmaBufferAllocate &GetInstance();

        UDmaBufferAllocate(const UDmaBufferAllocate &) = delete;
        UDmaBufferAllocate &operator=(const UDmaBufferAllocate &) = delete;
        UDmaBufferAllocate(UDmaBufferAllocate &&) = delete;
        UDmaBufferAllocate &operator=(UDmaBufferAllocate &&) = delete;

        bool AllocateBuffers(uint8_t num_buffers, uint32_t size_per_buffer, uint8_t quirk_mmap_mode = 2);
        bool AllocateBuffers(UdmaBufConfig cfg, uint8_t quirk_mmap_mode = 2);
        bool DeallocateBuffers(const std::vector<std::string> &buffer_names);

        std::vector<std::string> GetAllocatedBuffers() const;
        uint8_t GetAllocatedBuffersCount() const;
        bool IsModuleLoaded() const;

    private:
        UDmaBufferAllocate() = default;
        ~UDmaBufferAllocate();

        int m_ModuleFd{-1};
        bool m_IsModuleLoaded{false};
        std::vector<std::string> m_AllAllocatedBuffers;
        mutable std::mutex m_Mutex;

        bool LoadModule();
        bool UnloadModule();
        bool AllocateBuffersImpl(const UdmaBufConfig &cfg, uint8_t quirk_mmap_mode);
        std::string GetModulePath() const;
    };
}