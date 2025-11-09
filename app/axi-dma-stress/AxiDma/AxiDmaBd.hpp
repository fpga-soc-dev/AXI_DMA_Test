#pragma once

#include <array>
#include "AxiDmaHw.hpp"

// Use this to ensure the compiler doesn't add padding and
// aligns each struct to a 64-byte boundary.
#define DMA_BD_ALIGNMENT __attribute__((packed, aligned(64)))

namespace Dma
{
    using XAxiDma_Bd = std::uint32_t[XAXIDMA_BD_NUM_WORDS];

    struct XAxiDmaBd
    {
        volatile std::uint32_t next_bd_ptr_low;  // 0x00
        volatile std::uint32_t next_bd_ptr_high; // 0x04
        volatile std::uint32_t buffer_addr_low;  // 0x08
        volatile std::uint32_t buffer_addr_high; // 0x0C
        volatile std::uint32_t reserved1;        // 0x10
        volatile std::uint32_t reserved2;        // 0x14
        volatile std::uint32_t control;          // 0x18 (Length)
        volatile std::uint32_t status;           // 0x1C (Completed bit)
        volatile std::uint32_t app[5];           // 0x20 - 0x30 (Optional)
        volatile std::uint32_t reserved3[3];     // 0x34 - 0x3C (Padding to 64 bytes)
    } DMA_BD_ALIGNMENT;                          // Apply the alignment rules

    class AxiDmaBd
    {
    public:
        static bool XAxiDmaBdSetLength(std::uint32_t base_addr, std::uint32_t len_bytes, std::uint32_t len_mask);
        static bool XAxiDmaBdSetBufAddr(std::uint32_t base_addr, std::uintptr_t addr);
        static bool XAxiDmaBdSetBufAddrMicroMode(std::uint32_t base_addr, std::uintptr_t addr);
        static bool XAxiDmaBdSetAppWord(std::uint32_t base_addr, std::uint32_t offset, std::uint32_t value);
        static std::uint32_t XAxiDmaBdGetAppWord(std::uint32_t base_addr, std::uint32_t offset, int &valid);
        static void XAxiDmaBdSetCtrl(std::uint32_t base_addr, std::uint32_t ctrl);
        static void XAxiDmaBdDump(std::uint32_t base_addr);
        static void XAxiDmaBdClear(std::uint32_t base_addr);
        static std::uint32_t XAxiDmaBdRead(std::uint32_t base_addr, std::uint32_t offset);
        static void XAxiDmaBdWrite(std::uint32_t base_addr, std::uint32_t offset, std::uint32_t data);
        static void XAxiDmaBdWrite64(std::uint32_t base_addr, std::uint32_t offset, std::uint64_t data);
        static std::uint32_t XAxiDmaBdGetCtrl(std::uint32_t base_addr);
        static std::uint32_t XAxiDmaBdGetSts(std::uint32_t base_addr);
        static std::uint32_t XAxiDmaBdGetLength(std::uint32_t base_addr, std::uint32_t length_mask);
        static void XAxiDmaBdSetId(std::uint32_t base_addr, std::uint32_t id);
        static bool XAxiDmaBdHwCompleted(std::uint32_t base_addr);
    };
} // namespace Dma