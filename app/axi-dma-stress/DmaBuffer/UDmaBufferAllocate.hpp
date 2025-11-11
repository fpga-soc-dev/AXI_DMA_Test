#pragma once

#include <cstdint>
#include <string>
#include <cstddef>
#include <vector>
#include <string_view>

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

        UDmaBufferAllocate() = default;
        ~UDmaBufferAllocate();
        bool AllocateBuffers(uint8_t num_buffers, uint32_t size_per_buffer, uint8_t quirk_mmap_mode = 2);
        bool AllocateBuffers(UdmaBufConfig cfg, uint8_t quirk_mmap_mode = 2);
        std::vector<std::string> GetAllocatedBuffers();
        uint8_t GetAllocatedBuffersCount() const { return m_AllocatedBuffersCount; }

    private:
        int m_ModuleFd{-1};
        bool m_IsAllocated{false};
        uint8_t m_AllocatedBuffersCount{0};

        bool FreeBuffers();
    };
}