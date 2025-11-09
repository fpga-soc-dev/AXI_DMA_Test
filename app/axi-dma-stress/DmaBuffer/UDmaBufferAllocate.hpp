#pragma once

#include <cstdint>
#include <string>
#include <cstddef>
#include <vector>

namespace Dma
{
    class UDmaBufferAllocate
    {
    public:
        static constexpr const char *U_DMA_BUF_MODULE_NAME = "u-dma-buf.ko";
        static constexpr const char *U_DMA_BUF_LOADED_MODULE = "u_dma_buf";
        static constexpr const char *U_DMA_BUF_CLASS_PATH = "/sys/class/u-dma-buf";

        struct UdmaBufConfig
        {
            std::vector<std::pair<uint8_t, uint32_t>> buffer_vec;
        };

        UDmaBufferAllocate() = default;
        ~UDmaBufferAllocate();
        bool AllocateBuffers(uint8_t num_buffers, uint32_t size_per_buffer, uint8_t quirk_mmap_mode = 2);
        bool AllocateBuffers(UdmaBufConfig buffer_vec, uint8_t quirk_mmap_mode = 2);
        bool FreeBuffers();
        std::vector<std::string> GetAllocatedBuffers();
        uint8_t GetAllocatedBuffersCount() const { return m_AllocatedBuffersCount; }

    private:
        int m_ModuleFd{-1};
        bool m_IsAllocated{false};
        uint8_t m_AllocatedBuffersCount{0};
    };
}