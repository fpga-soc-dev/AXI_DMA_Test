#include <spdlog/spdlog.h>
#include "AxiDmaBd.hpp"
#include "Uio.hpp"

namespace Dma
{

    bool AxiDmaBd::XAxiDmaBdSetLength(std::uint32_t base_addr, std::uint32_t len_bytes, std::uint32_t len_mask)
    {
        if (len_bytes == 0 || len_bytes > len_mask)
        {
            spdlog::error("Invalid length: {} (mask: {})", len_bytes, len_mask);
            return false;
        }

        auto reg = XAxiDmaBdRead(base_addr, XAXIDMA_BD_CTRL_LEN_OFFSET);
        reg = (reg & ~len_mask) | len_bytes;
        XAxiDmaBdWrite(base_addr, XAXIDMA_BD_CTRL_LEN_OFFSET, reg);

        return true;
    }

    bool AxiDmaBd::XAxiDmaBdSetBufAddr(std::uint32_t base_addr, std::uintptr_t addr)
    {
        const std::uint32_t has_dre = XAxiDmaBdRead(base_addr, XAXIDMA_BD_HAS_DRE_OFFSET);
        const std::uint8_t word_len = static_cast<std::uint8_t>(has_dre & XAXIDMA_BD_WORDLEN_MASK);

        if (addr & (word_len - 1))
        {
            if ((has_dre & XAXIDMA_BD_HAS_DRE_MASK) == 0)
            {
                SPDLOG_ERROR("Error set buf addr 0x{:X} with 0x{:X} and 0x{:X}, 0x{:X}",
                             addr, has_dre, (word_len - 1), addr & (word_len - 1));
                return false;
            }
        }

#if defined(__aarch64__) || defined(__arch64__)
        XAxiDmaBdWrite64(base_addr, XAXIDMA_BD_BUFA_OFFSET, addr);
#else
        XAxiDmaBdWrite(base_addr, XAXIDMA_BD_BUFA_OFFSET, static_cast<std::uint32_t>(addr));
#endif

        return true;
    }

    bool AxiDmaBd::XAxiDmaBdSetBufAddrMicroMode(std::uint32_t base_addr, std::uintptr_t addr)
    {
        if (addr & XAXIDMA_MICROMODE_MIN_BUF_ALIGN)
        {
            SPDLOG_ERROR("Error set buf addr 0x{:X} and 0x{:X}, 0x{:X}",
                         static_cast<unsigned>(addr),
                         XAXIDMA_MICROMODE_MIN_BUF_ALIGN,
                         static_cast<unsigned>(addr & XAXIDMA_MICROMODE_MIN_BUF_ALIGN));
            return false;
        }

#if defined(__aarch64__) || defined(__arch64__)
        XAxiDmaBdWrite64(base_addr, XAXIDMA_BD_BUFA_OFFSET, addr);
#else
        XAxiDmaBdWrite(base_addr, XAXIDMA_BD_BUFA_OFFSET, static_cast<std::uint32_t>(addr));
#endif

        return true;
    }

    bool AxiDmaBd::XAxiDmaBdSetAppWord(std::uint32_t base_addr, std::uint32_t offset, std::uint32_t value)
    {
        if (XAxiDmaBdRead(base_addr, XAXIDMA_BD_HAS_STSCNTRL_OFFSET) == 0)
        {
            SPDLOG_ERROR("Error set app word at offset 0x{:X}, no stscntrl strm", offset);
            return false;
        }

        if (offset < 0 || offset > XAXIDMA_LAST_APPWORD)
        {
            SPDLOG_ERROR("Error set app word at invalid offset 0x{:X}", offset);
            return false;
        }

        XAxiDmaBdWrite(base_addr, XAXIDMA_BD_USR0_OFFSET + (offset * 4), value);

        return true;
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetAppWord(std::uint32_t base_addr, std::uint32_t offset, int &valid)
    {
        valid = 0;

        if (XAxiDmaBdRead(base_addr, XAXIDMA_BD_HAS_STSCNTRL_OFFSET) == 0)
        {
            SPDLOG_ERROR("Error get app word at offset 0x{:X}, no stscntrl strm", offset);
            return 0;
        }

        if (offset < 0 || offset > XAXIDMA_LAST_APPWORD)
        {
            SPDLOG_ERROR("Error get app word at invalid offset 0x{:X}", offset);
            return 0;
        }

        valid = 1;

        return XAxiDmaBdRead(base_addr, XAXIDMA_BD_USR0_OFFSET + (offset * 4));
    }

    void AxiDmaBd::XAxiDmaBdSetCtrl(std::uint32_t base_addr, std::uint32_t ctrl)
    {
        auto reg_value = XAxiDmaBdRead(base_addr, XAXIDMA_BD_CTRL_LEN_OFFSET);
        reg_value &= ~XAXIDMA_BD_CTRL_ALL_MASK;
        reg_value |= (ctrl & XAXIDMA_BD_CTRL_ALL_MASK);
        XAxiDmaBdWrite(base_addr, XAXIDMA_BD_CTRL_LEN_OFFSET, reg_value);
        return;
    }

    void AxiDmaBd::XAxiDmaBdDump(std::uint32_t base_addr)
    {
        SPDLOG_INFO("Buffer Descriptor Dump:");
        SPDLOG_INFO("\tNext Bd Ptr: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_NDESC_OFFSET));
        SPDLOG_INFO("\tBuff addr: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_BUFA_OFFSET));
        SPDLOG_INFO("\tMCDMA Fields: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_MCCTL_OFFSET));
        SPDLOG_INFO("\tVSIZE_STRIDE: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_STRIDE_VSIZE_OFFSET));
        SPDLOG_INFO("\tContrl len: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_CTRL_LEN_OFFSET));
        SPDLOG_INFO("\tStatus: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_STS_OFFSET));

        SPDLOG_INFO("\tAPP 0: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_USR0_OFFSET));
        SPDLOG_INFO("\tAPP 1: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_USR1_OFFSET));
        SPDLOG_INFO("\tAPP 2: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_USR2_OFFSET));
        SPDLOG_INFO("\tAPP 3: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_USR3_OFFSET));
        SPDLOG_INFO("\tAPP 4: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_USR4_OFFSET));
        SPDLOG_INFO("\tSW ID: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_ID_OFFSET));
        SPDLOG_INFO("\tStsCtrl: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_HAS_STSCNTRL_OFFSET));
        SPDLOG_INFO("\tDRE: 0x{:08X}", XAxiDmaBdRead(base_addr, XAXIDMA_BD_HAS_DRE_OFFSET));
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdRead(std::uint32_t base_addr, uint32_t offset)
    {
        auto *ptr = reinterpret_cast<volatile std::uint32_t *>(base_addr + static_cast<std::uintptr_t>(offset));
        return *ptr;
    }

    void AxiDmaBd::XAxiDmaBdWrite(std::uint32_t base_addr, uint32_t offset, uint32_t data)
    {
        auto *ptr = reinterpret_cast<volatile std::uint32_t *>(
            static_cast<std::uintptr_t>(base_addr) + static_cast<std::uintptr_t>(offset));

        *ptr = data;
    }

    void AxiDmaBd::XAxiDmaBdWrite64(std::uint32_t base_addr, uint32_t offset, uint64_t data)
    {
        auto *ptr = reinterpret_cast<volatile std::uint64_t *>(base_addr + static_cast<std::uintptr_t>(offset));
        *ptr = data;
    }

    void AxiDmaBd::XAxiDmaBdClear(std::uint32_t base_addr)
    {
        auto *start_ptr = reinterpret_cast<volatile std::uint8_t *>(base_addr + XAXIDMA_BD_START_CLEAR);

        for (std::size_t i = 0; i < XAXIDMA_BD_BYTES_TO_CLEAR; ++i)
        {
            start_ptr[i] = 0;
        }
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetCtrl(std::uint32_t base_addr)
    {
        return XAxiDmaBdRead(base_addr, XAXIDMA_BD_CTRL_LEN_OFFSET) & XAXIDMA_BD_CTRL_ALL_MASK;
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetSts(std::uint32_t base_addr)
    {
        return XAxiDmaBdRead(base_addr, XAXIDMA_BD_STS_OFFSET) & XAXIDMA_BD_STS_ALL_MASK;
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetLength(std::uint32_t base_addr, std::uint32_t length_mask)
    {
        return XAxiDmaBdRead(base_addr, XAXIDMA_BD_CTRL_LEN_OFFSET) & length_mask;
    }

    void AxiDmaBd::XAxiDmaBdSetId(std::uint32_t base_addr, std::uint32_t id)
    {
        XAxiDmaBdWrite(base_addr, XAXIDMA_BD_ID_OFFSET, id);
    }

    bool AxiDmaBd::XAxiDmaBdHwCompleted(std::uint32_t base_addr)
    {
        auto status = XAxiDmaBdRead(base_addr, XAXIDMA_BD_STS_OFFSET);
        return (status & XAXIDMA_BD_STS_COMPLETE_MASK) != 0;
    }

} // namespace Dma