
#include <spdlog/spdlog.h>
#include "AxiDmaBd.hpp"

namespace Dma
{

    AxiDmaBd::AxiDmaBd() : m_Base(nullptr)
    {
    }

    void AxiDmaBd::SetBaseAddress(std::uint8_t *base_addr)
    {
        m_Base = base_addr;
    }

    std::uint8_t *AxiDmaBd::GetBaseAddress() const
    {
        return m_Base;
    }

    bool AxiDmaBd::XAxiDmaBdSetLength(std::uint32_t len_bytes, std::uint32_t len_mask)
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error set length 0x{:X}, bd base is null", len_bytes);
            return false;
        }

        if (len_bytes == 0 || len_bytes > len_mask)
        {
            SPDLOG_ERROR("Error set length 0x{:X}, exceeds mask 0x{:X}", len_bytes, len_mask);
            return false;
        }

        auto reg_value = XAxiDmaBdRead(XAXIDMA_BD_CTRL_LEN_OFFSET);
        reg_value = (reg_value & ~len_mask) | len_bytes;
        XAxiDmaBdWrite(XAXIDMA_BD_CTRL_LEN_OFFSET, reg_value);

        return true;
    }

    bool AxiDmaBd::XAxiDmaBdSetBufAddr(std::uint64_t phys_addr)
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error set buf addr 0x{:X}, bd base is null", phys_addr);
            return false;
        }

        const std::uint32_t has_dre = XAxiDmaBdRead(XAXIDMA_BD_HAS_DRE_OFFSET);
        const std::uint8_t word_len = static_cast<std::uint8_t>(has_dre & XAXIDMA_BD_WORDLEN_MASK);

        if (word_len != 0)
        {
            if (phys_addr & (word_len - 1))
            {
                if ((has_dre & XAXIDMA_BD_HAS_DRE_MASK) == 0)
                {
                    SPDLOG_ERROR("Error set buf addr 0x{:X} with 0x{:X} and 0x{:X}, 0x{:X}",
                                 phys_addr, has_dre, (word_len - 1), phys_addr & (word_len - 1));
                    return false;
                }
            }
        }
#if defined(__aarch64__) || defined(__arch64__)
        XAxiDmaBdWrite64(XAXIDMA_BD_BUFA_OFFSET, phys_addr);
#else
        XAxiDmaBdWrite(XAXIDMA_BD_BUFA_OFFSET, phys_addr);
#endif

        return true;
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetBufAddr()
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error get buf addr, bd base is null");
            return 0;
        }

        return (XAxiDmaBdRead(XAXIDMA_BD_BUFA_OFFSET));
    }

    bool AxiDmaBd::XAxiDmaBdSetBufAddrMicroMode(std::uint64_t phys_addr)
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error set buf addr 0x{:X}, bd base is null", phys_addr);
            return false;
        }

        if (phys_addr & XAXIDMA_MICROMODE_MIN_BUF_ALIGN)
        {
            SPDLOG_ERROR("XAxiDmaBdSetBufAddrMicroMode: addr 0x%llx not 4K aligned\n",
                         static_cast<unsigned long long>(phys_addr));
            return false;
        }

#if defined(__aarch64__) || defined(__arch64__)
        XAxiDmaBdWrite64(XAXIDMA_BD_BUFA_OFFSET, phys_addr);
#else
        XAxiDmaBdWrite(XAXIDMA_BD_BUFA_OFFSET, phys_addr);
#endif

        return true;
    }

    bool AxiDmaBd::XAxiDmaBdSetAppWord(std::uint32_t index, std::uint32_t value)
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error set app word at offset 0x{:X}, bd base is null", index);
            return false;
        }

        if (XAxiDmaBdRead(XAXIDMA_BD_HAS_STSCNTRL_OFFSET) == 0)
        {
            SPDLOG_ERROR("Error set app word at offset 0x{:X}, no stscntrl strm", index);
            return false;
        }

        if (index >= XAXIDMA_LAST_APPWORD)
        {
            SPDLOG_ERROR("Error set app word at invalid offset 0x{:X}", index);
            return false;
        }

        const std::uint32_t offset = XAXIDMA_BD_USR0_OFFSET + index * 4U;
        XAxiDmaBdWrite(offset, value);

        return true;
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetAppWord(std::uint32_t index, int &valid)
    {
        valid = 0;
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error get app word at offset 0x{:X}, bd base is null", index);
            return 0;
        }

        if (XAxiDmaBdRead(XAXIDMA_BD_HAS_STSCNTRL_OFFSET) == 0)
        {
            SPDLOG_ERROR("Error get app word at offset 0x{:X}, no stscntrl strm", index);
            return 0;
        }

        if (index > XAXIDMA_LAST_APPWORD)
        {
            SPDLOG_ERROR("Error get app word at invalid offset 0x{:X}", index);
            return 0;
        }

        valid = 1;

        const std::uint32_t offset = XAXIDMA_BD_USR0_OFFSET + index * 4U;
        return XAxiDmaBdRead(offset);
    }

    void AxiDmaBd::XAxiDmaBdSetCtrl(std::uint32_t ctrl)
    {
        auto reg_value = XAxiDmaBdRead(XAXIDMA_BD_CTRL_LEN_OFFSET);
        reg_value &= ~XAXIDMA_BD_CTRL_ALL_MASK;
        reg_value |= (ctrl & XAXIDMA_BD_CTRL_ALL_MASK);
        XAxiDmaBdWrite(XAXIDMA_BD_CTRL_LEN_OFFSET, reg_value);
        return;
    }

    bool AxiDmaBd::XAxiDmaBdSetNextDescOffset(std::uint64_t phys_addr)
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error set next desc addr 0x{:X}, bd base is null", phys_addr);
            return false;
        }

        XAxiDmaBdWrite(XAXIDMA_BD_NDESC_OFFSET, phys_addr & 0xFFFFFFFF);
        XAxiDmaBdWrite(XAXIDMA_BD_NDESC_MSB_OFFSET, ((phys_addr >> 32) & 0xFFFFFFFF));

        return true;
    }

    void AxiDmaBd::XAxiDmaBdDump()
    {
        SPDLOG_TRACE("=== AXI DMA BD @ %p ===\n", static_cast<void *>(m_Base));
        SPDLOG_TRACE("\tNext Bd Ptr: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_NDESC_OFFSET));
        SPDLOG_TRACE("\tBuff addr: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_BUFA_OFFSET));
        SPDLOG_TRACE("\tMCDMA Fields: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_MCCTL_OFFSET));
        SPDLOG_TRACE("\tVSIZE_STRIDE: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_STRIDE_VSIZE_OFFSET));
        SPDLOG_TRACE("\tContrl len: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_CTRL_LEN_OFFSET));
        SPDLOG_TRACE("\tStatus: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_STS_OFFSET));
        SPDLOG_TRACE("\tAPP 0: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_USR0_OFFSET));
        SPDLOG_TRACE("\tAPP 1: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_USR1_OFFSET));
        SPDLOG_TRACE("\tAPP 2: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_USR2_OFFSET));
        SPDLOG_TRACE("\tAPP 3: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_USR3_OFFSET));
        SPDLOG_TRACE("\tAPP 4: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_USR4_OFFSET));
        SPDLOG_TRACE("\tSW ID: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_ID_OFFSET));
        SPDLOG_TRACE("\tStsCtrl: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_HAS_STSCNTRL_OFFSET));
        SPDLOG_TRACE("\tDRE: 0x{:08X}", XAxiDmaBdRead(XAXIDMA_BD_HAS_DRE_OFFSET));
    }

    void AxiDmaBd::XAxiDmaBdClear()
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error clear bd, bd base is null");
            return;
        }

        std::memset(reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(m_Base) + XAXIDMA_BD_START_CLEAR), 0, XAXIDMA_BD_NUM_WORDS);
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetCtrl()
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error get ctrl, bd base is null");
            return 0;
        }
        return XAxiDmaBdRead(XAXIDMA_BD_CTRL_LEN_OFFSET) & XAXIDMA_BD_CTRL_ALL_MASK;
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetSts()
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error get sts, bd base is null");
            return 0;
        }
        return XAxiDmaBdRead(XAXIDMA_BD_STS_OFFSET) & XAXIDMA_BD_STS_ALL_MASK;
    }

    void AxiDmaBd::XAxiDmaBdSetSts(std::uint32_t sts)
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error set sts 0x{:X}, bd base is null", sts);
            return;
        }
        XAxiDmaBdWrite(XAXIDMA_BD_STS_OFFSET, sts);
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdGetLength(std::uint32_t length_mask)
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error get length, bd base is null");
            return 0;
        }
        return XAxiDmaBdRead(XAXIDMA_BD_CTRL_LEN_OFFSET) & length_mask;
    }

    void AxiDmaBd::XAxiDmaBdSetId(std::uint32_t id)
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error set id 0x{:X}, bd base is null", id);
            return;
        }
        XAxiDmaBdWrite(XAXIDMA_BD_ID_OFFSET, id);
    }

    bool AxiDmaBd::XAxiDmaBdHwCompleted()
    {
        if (m_Base == nullptr)
        {
            SPDLOG_ERROR("Error check hw completed, bd base is null");
            return false;
        }
        auto status = XAxiDmaBdRead(XAXIDMA_BD_STS_OFFSET);
        return (status & XAXIDMA_BD_STS_COMPLETE_MASK) != 0;
    }

    std::uint32_t AxiDmaBd::XAxiDmaBdRead(std::uint32_t offset) const
    {
        const auto *p = reinterpret_cast<const std::uint32_t *>(m_Base + offset);
        return *p;
    }

    void AxiDmaBd::XAxiDmaBdWrite(std::uint32_t offset, std::uint32_t data)
    {
        auto *p = reinterpret_cast<std::uint32_t *>(m_Base + offset);
        *p = data;
    }

    void AxiDmaBd::XAxiDmaBdWrite64(std::uint32_t offset, std::uint64_t data)
    {
        XAxiDmaBdWrite(offset, static_cast<std::uint32_t>(data & 0xFFFFFFFFULL));
        XAxiDmaBdWrite(offset + 4U, static_cast<std::uint32_t>(data >> 32));
    }

} // namespace Dma
