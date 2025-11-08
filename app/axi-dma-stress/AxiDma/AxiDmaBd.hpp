#pragma once

#include <array>
#include "AxiDmaHw.hpp"

namespace Dma
{
    using XAxiDma_Bd = std::uint32_t[XAXIDMA_BD_NUM_WORDS];

    class AxiDmaBd
    {
    public:
        explicit AxiDmaBd(XAxiDma_Bd *bd_baseptr);
        ~AxiDmaBd() = default;

        bool XAxiDmaBdSetLength(std::uint32_t len_bytes, std::uint32_t len_mask);
        bool XAxiDmaBdSetBufferAddr(std::uintptr_t addr);
        bool XAxiDmaBdSetBufAddrMicroMode(std::uintptr_t addr);
        bool XAxiDmaBdSetAppWord(std::uint32_t offset, std::uint32_t value);
        std::uint32_t XAxiDmaBdGetAppWord(std::uint32_t offset, int &valid);
        void XAxiDmaBdSetCtrl(std::uint32_t ctrl);
        void XAxiDmaBdDump();

    private:
        XAxiDma_Bd *m_BdBaseptr;

        std::uint32_t XAxiDmaBdRead(std::uint32_t offset) const noexcept;
        void XAxiDmaBdWrite(std::uint32_t offset, std::uint32_t data) noexcept;
        void XAxiDmaBdWrite64(std::uint32_t offset, std::uint64_t data) noexcept;
        void XAxiDmaBdClear() noexcept;
        std::uint32_t XAxiDmaBdGetCtrl() const noexcept;
        std::uint32_t XAxiDmaBdGetSts() const noexcept;
        std::uint32_t XAxiDmaBdGetLength(std::uint32_t length_mask) const noexcept;
    };
} // namespace Dma