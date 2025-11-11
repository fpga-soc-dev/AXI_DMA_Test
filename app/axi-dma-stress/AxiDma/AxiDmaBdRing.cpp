#include "AxiDmaBdRing.hpp"

#include <spdlog/spdlog.h>
#include <memory>

namespace Dma
{

    XAxiDmaBdRing::XAxiDmaBdRing(std::shared_ptr<AxiDmaChannel> channel, const UDmaBuffer &udma_buffer) : m_Channel(channel),
                                                                                                          m_UdmaBuffer(udma_buffer)
    {
    }

    XAxiDmaBdRing::~XAxiDmaBdRing()
    {
        // delete[] m_CyclicBd;
        // m_CyclicBd = nullptr;
    }

    void XAxiDmaBdRing::XAxiDmaBdRingSnapShotCurrBd()
    {
        // TX Channel
        if (!m_IsRxChannel)
        {
            if (!m_Addr_ext)
            {
                auto reg_val = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET);
                std::uint8_t *bd_ptr = reinterpret_cast<std::uint8_t *>(reg_val);
                m_BdRestart.SetBaseAddress(bd_ptr);
            }
            else
            {
                auto lower_addr = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET);
                auto upper_addr = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CDESC_MSB_OFFSET);
                auto bd_addr = static_cast<std::uintptr_t>(lower_addr) | (static_cast<std::uintptr_t>(upper_addr) << 32);
                std::uint8_t *bd_ptr = reinterpret_cast<std::uint8_t *>(bd_addr);
                m_BdRestart.SetBaseAddress(bd_ptr);
            }
            return;
        }

        // RX Channel
        if (!m_RingIndex)
        {
            // first RX ring
            if (!m_Addr_ext)
            {
                auto reg_value = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET);
                std::uint8_t *bd_ptr = reinterpret_cast<std::uint8_t *>(static_cast<std::uintptr_t>(reg_value));
                m_BdRestart.SetBaseAddress(bd_ptr);
            }
            else
            {
                auto lower_addr = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET);
                auto upper_addr = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CDESC_MSB_OFFSET);
                auto bd_addr = static_cast<std::uintptr_t>(lower_addr) | (static_cast<std::uintptr_t>(upper_addr) << 32);
                std::uint8_t *bd_ptr = reinterpret_cast<std::uint8_t *>(bd_addr);
                m_BdRestart.SetBaseAddress(bd_ptr);
            }
        }
        else
        {
            if (!m_Addr_ext)
            {
                auto off = XAXIDMA_RX_CDESC0_OFFSET + (m_RingIndex - 1) * XAXIDMA_RX_NDESC_OFFSET;
                auto reg_value = m_Channel->ReadReg(off);
                std::uint8_t *bd_ptr = reinterpret_cast<std::uint8_t *>(static_cast<std::uintptr_t>(reg_value));
                m_BdRestart.SetBaseAddress(bd_ptr);
            }
            else
            {
                auto lower_addr = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET);
                auto upper_addr = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_RX_CDESC0_MSB_OFFSET + (m_RingIndex - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET);
                auto bd_addr = static_cast<std::uintptr_t>(lower_addr) | (static_cast<std::uintptr_t>(upper_addr) << 32);
                std::uint8_t *bd_ptr = reinterpret_cast<std::uint8_t *>(bd_addr);
                m_BdRestart.SetBaseAddress(bd_ptr);
            }
        }
    }

    std::uint32_t XAxiDmaBdRing::XAxiDmaBdRingCntCalc(std::uint32_t alignment, std::uint32_t buffer_size)
    {
        std::uint32_t alignedSize = (sizeof(XAxiDma_Bd) + (alignment - 1)) & ~((alignment)-1);
        return buffer_size / alignedSize;
    }

    bool XAxiDmaBdRing::XAxiDmaBdRingCreate(std::uintptr_t phys_addr, std::uintptr_t virt_addr, std::uint32_t alignment, int bd_count)
    {
        if (bd_count <= 0)
        {
            SPDLOG_ERROR("BdRingCreate: non-positive BD number {}", bd_count);
            return false;
        }

        /* In case there is a failure prior to creating list, make sure the
         * following attributes are 0 to prevent calls to other SG functions
         * from doing anything
         */
        m_AllCnt = 0;
        m_FreeCnt = 0;
        m_HwCnt = 0;
        m_PreCnt = 0;
        m_PostCnt = 0;
        m_Cyclic = 0;

        /* Make sure Alignment parameter meets minimum requirements */
        if (alignment < XAXIDMA_BD_MINIMUM_ALIGNMENT)
        {
            SPDLOG_ERROR("BdRingCreate: alignment too small {}, need to be at least {}", alignment, XAXIDMA_BD_MINIMUM_ALIGNMENT);
            return false;
        }

        /* Make sure Alignment is a power of 2 */
        if ((alignment - 1) & alignment)
        {

            SPDLOG_ERROR("BdRingCreate: alignment not valid {}", (int)alignment);
            return false;
        }

        /* Make sure PhysAddr and VirtAddr are on same Alignment */
        if ((phys_addr % alignment) || (virt_addr % alignment))
        {
            SPDLOG_ERROR("BdRingCreate: Physical address {} and virtual address {} have different alignment", (std::uintptr_t)phys_addr, (std::uintptr_t)virt_addr);
            return false;
        }

        /* Compute how many bytes will be between the start of adjacent BDs */
        m_Separation = (sizeof(XAxiDma_Bd) + (alignment - 1)) & ~(alignment - 1);
        SPDLOG_DEBUG("BD Separation set to {} bytes", m_Separation);

        /* Must make sure the ring doesn't span address 0x00000000. If it does,
         * then the next/prev BD traversal macros will fail.
         */
        if (virt_addr > (virt_addr + (m_Separation * bd_count) - 1))
        {
            SPDLOG_ERROR("BdRingCreate: BD space cross 0x0\r\n");
            return false;
        }

        /* Initial ring setup:
         *  - Clear the entire space
         *  - Setup each BD's next pointer with the physical address of the
         *    next BD
         *  - Put hardware information in each BD
         */
        std::memset(reinterpret_cast<void *>(virt_addr), 0, (m_Separation * bd_count));

        auto bd_virt_addr = virt_addr;
        auto bd_phys_addr = phys_addr + m_Separation;

        for (int i = 1; i < bd_count; i++)
        {
            AxiDmaBd::XAxiDmaBdWrite(bd_virt_addr, XAXIDMA_BD_NDESC_OFFSET, static_cast<std::uint32_t>(bd_phys_addr & XAXIDMA_DESC_LSB_MASK));

            // Write next descriptor pointer (MSB for 64-bit addressing)
            AxiDmaBd::XAxiDmaBdWrite(bd_virt_addr, XAXIDMA_BD_NDESC_MSB_OFFSET, static_cast<std::uint32_t>((bd_phys_addr >> 32) & 0xFFFFFFFF));

            /* Put hardware information in the BDs */
            AxiDmaBd::XAxiDmaBdWrite(bd_virt_addr, XAXIDMA_BD_HAS_STSCNTRL_OFFSET, static_cast<std::uint32_t>(m_HasStsCntrlStrm));

            AxiDmaBd::XAxiDmaBdWrite(bd_virt_addr, XAXIDMA_BD_HAS_DRE_OFFSET, ((static_cast<std::uint32_t>(m_HasDRE) << XAXIDMA_BD_HAS_DRE_SHIFT) | m_DataWidth));

            bd_virt_addr += m_Separation;
            bd_phys_addr += m_Separation;
        }

        /* At the end of the ring, link the last BD back to the top */
        AxiDmaBd::XAxiDmaBdWrite(bd_virt_addr, XAXIDMA_BD_NDESC_OFFSET, static_cast<std::uint32_t>(phys_addr & XAXIDMA_DESC_LSB_MASK));

        AxiDmaBd::XAxiDmaBdWrite(bd_virt_addr, XAXIDMA_BD_NDESC_MSB_OFFSET, static_cast<std::uint32_t>((phys_addr >> 32) & 0xFFFFFFFF));

        /* Setup the last BD's hardware information */
        AxiDmaBd::XAxiDmaBdWrite(bd_virt_addr, XAXIDMA_BD_HAS_STSCNTRL_OFFSET, static_cast<std::uint32_t>(m_HasStsCntrlStrm));

        AxiDmaBd::XAxiDmaBdWrite(bd_virt_addr, XAXIDMA_BD_HAS_DRE_OFFSET, ((static_cast<std::uint32_t>(m_HasDRE) << XAXIDMA_BD_HAS_DRE_SHIFT) | m_DataWidth));

        /* Setup and initialize pointers and counters */
        m_RunState = AXIDMA_CHANNEL_HALTED;
        m_FirstBdAddr = virt_addr;
        m_FirstBdPhysAddr = phys_addr;
        m_LastBdAddr = bd_virt_addr;
        m_LastBdPhysAddr = phys_addr + ((bd_count - 1) * m_Separation);
        m_Length = m_LastBdAddr - m_FirstBdAddr + m_Separation;
        m_AllCnt = bd_count;
        m_FreeCnt = bd_count;

        // Initialize all BD pointers to start of ring
        std::uint8_t *bd_ptr = reinterpret_cast<std::uint8_t *>(virt_addr);
        m_FreeHead.SetBaseAddress(bd_ptr);
        m_PreHead.SetBaseAddress(bd_ptr);
        m_HwHead.SetBaseAddress(bd_ptr);
        m_HwTail.SetBaseAddress(bd_ptr);
        m_PostHead.SetBaseAddress(bd_ptr);
        m_BdRestart.SetBaseAddress(bd_ptr);

        // m_CyclicBd = new XAxiDma_Bd[]();

        SPDLOG_INFO("BD Ring created successfully: {} BDs, separation={} bytes, total size={} bytes", bd_count, m_Separation, m_Length);

        return true;
    }

    bool XAxiDmaBdRing::XAxiDmaBdRingClone(XAxiDma_Bd *src_bd_ptr)
    {
        if (m_AllCnt == 0)
        {
            SPDLOG_ERROR("BdRingClone: BD Ring not created yet");
            return false;
        }

        if (m_RunState == AXIDMA_CHANNEL_NOT_HALTED)
        {
            SPDLOG_ERROR("BdRingClone: bd ring started already, cannot do");
            return false;
        }

        if (m_FreeCnt != m_AllCnt)
        {
            SPDLOG_ERROR("BdRingClone: some bds already in use {}/{}", m_FreeCnt, m_AllCnt);
            return false;
        }

        /* Make a copy of the template then modify it by clearing the complete bit in status/control field */
        XAxiDma_Bd tmp_bd{};
        std::memcpy(tmp_bd, *src_bd_ptr, sizeof(XAxiDma_Bd));

        std::uintptr_t tmp_bd_base = reinterpret_cast<std::uintptr_t>(&tmp_bd[0]);

        std::uint32_t save = AxiDmaBd::XAxiDmaBdRead(tmp_bd_base, XAXIDMA_BD_STS_OFFSET);
        save &= ~XAXIDMA_BD_STS_COMPLETE_MASK;
        AxiDmaBd::XAxiDmaBdWrite(tmp_bd_base, XAXIDMA_BD_STS_OFFSET, save);

        /* Clone the template BD to all BDs in the ring */
        std::uintptr_t cur_bd = m_FirstBdAddr;
        for (int i = 0; i < m_AllCnt; i++, cur_bd += m_Separation)
        {

            // Copy the relevant portion of the BD (excluding next pointer and other hardware fields)
            void *dest_addr = reinterpret_cast<void *>(cur_bd + XAXIDMA_BD_START_CLEAR);
            void *src_addr = reinterpret_cast<void *>(tmp_bd_base + XAXIDMA_BD_START_CLEAR);

            std::memcpy(dest_addr, src_addr, XAXIDMA_BD_BYTES_TO_CLEAR);
        }

        SPDLOG_INFO("BD Ring cloned successfully from template BD");
        return true;
    }

    bool XAxiDmaBdRing::XAxiDmaBdRingAlloc(int num_bd, XAxiDma_Bd **bd_ptr)
    {
        if (num_bd <= 0)
        {
            SPDLOG_ERROR("BdRingAlloc: non-positive BD number {}", num_bd);
            return false;
        }

        if (num_bd > m_FreeCnt)
        {
            SPDLOG_ERROR("BdRingAlloc: not enough free BDs (requested {}, available {})", num_bd, m_FreeCnt);
            return false;
        }

        *bd_ptr = m_FreeHead.GetBaseAddress();

        XAxiDmaRingSeekAhead(m_FreeHead, num_bd);

        m_FreeCnt -= num_bd;
        m_PreCnt += num_bd;

        SPDLOG_INFO("Allocated {} BDs from ring, {} BDs remaining free", num_bd, m_FreeCnt);
        return true;
    }

    void XAxiDmaBdRing::XAxiDmaRingSeekAhead(AxiDmaBd &bd_ptr, int num_bd)
    {
        std::uint8_t *current_ptr = bd_ptr.GetBaseAddress();
        auto addr = reinterpret_cast<std::uintptr_t>(current_ptr);

        addr += static_cast<std::uintptr_t>(m_Separation) *
                static_cast<std::uintptr_t>(num_bd);

        if ((addr > m_LastBdAddr) ||
            (reinterpret_cast<std::uintptr_t>(current_ptr) > addr))
        {
            addr -= m_Length;
        }

        std::uint8_t *new_ptr = reinterpret_cast<std::uint8_t *>(addr);
        bd_ptr.SetBaseAddress(new_ptr);
    }

    std::uint8_t *XAxiDmaBdRing::XAxiDmaBdRingNext(const std::uint8_t *bd)
    {
        std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(bd);

        addr += static_cast<std::uintptr_t>(m_Separation);

        if (addr > m_LastBdAddr)
        {
            addr = m_FirstBdAddr;
        }

        return reinterpret_cast<std::uint8_t *>(addr);
    }

    bool XAxiDmaBdRing::XAxiDmaBdRingSetCoalesce(std::uint32_t counter, std::uint32_t timer)
    {
        std::uint32_t control_reg = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CR_OFFSET);

        if (counter != XAXIDMA_NO_CHANGE)
        {
            if ((counter == 0) || (counter > 0xFF))
            {
                SPDLOG_ERROR("BdRingSetCoalesce: invalid  coalescing threshold {}", counter);
                return false;
            }
            control_reg = (control_reg & ~XAXIDMA_COALESCE_MASK) | (counter << XAXIDMA_COALESCE_SHIFT);
        }

        if (timer != XAXIDMA_NO_CHANGE)
        {
            if (timer > 0xFF)
            {
                SPDLOG_ERROR("BdRingSetCoalesce: invalid delay timer {}", timer);
                return false;
            }
            control_reg = (control_reg & ~XAXIDMA_DELAY_MASK) | (timer << XAXIDMA_DELAY_SHIFT);
        }

        m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CR_OFFSET, control_reg);

        return true;
    }

    bool XAxiDmaBdRing::XAxiDmaBdRingToHw(int num_bd, std::uint8_t *bd_set_ptr)
    {
        if (num_bd < 0)
        {
            SPDLOG_ERROR("BdRingToHw: non-positive BD number {}", num_bd);
            return false;
        }

        if (num_bd == 0)
        {
            SPDLOG_DEBUG("BdRingToHw: zero BD number, nothing to do");
            return true;
        }

        if ((num_bd > m_PreCnt) || (m_PreHead.GetBaseAddress() != bd_set_ptr))
        {
            SPDLOG_ERROR("BdRingToHw: not enough BDs in pre-work group (requested {}, available {})", num_bd, m_PreCnt);
            return false;
        }

        // Update HwTail to point to the last BD being submitted to hardware
        std::uint8_t *cur_bd_ptr = bd_set_ptr;
        std::uint32_t bd_cr = AxiDmaBd::XAxiDmaBdGetCtrl(reinterpret_cast<std::uintptr_t>(cur_bd_ptr));
        std::uint32_t bd_sts = AxiDmaBd::XAxiDmaBdGetSts(reinterpret_cast<std::uintptr_t>(cur_bd_ptr));

        /* In case of Tx channel, the first BD should have been marked as start-of-frame*/
        if (!(m_IsRxChannel) && !(bd_cr & XAXIDMA_BD_CTRL_TXSOF_MASK))
        {
            SPDLOG_ERROR("BdRingToHw: first BD in the set is not marked as start-of-frame");
            return false;
        }

        /* Clear the completed status bit*/
        for (int i = 0; i < num_bd; i++)
        {
            /* Make sure the length value in the BD is non-zero. */
            if (AxiDmaBd::XAxiDmaBdGetLength(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), m_MaxTransferLen) == 0)
            {
                SPDLOG_ERROR("BdRingToHw: BD {} has zero length", i);
                return false;
            }

            bd_sts &= ~XAXIDMA_BD_STS_COMPLETE_MASK;
            AxiDmaBd::XAxiDmaBdWrite(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), XAXIDMA_BD_STS_OFFSET, bd_sts);

            /* Flush the current BD so DMA core could see the updates */
            // XAXIDMA_CACHE_FLUSH(CurBdPtr);

            cur_bd_ptr = XAxiDmaBdRingNext(cur_bd_ptr);
            bd_cr = AxiDmaBd::XAxiDmaBdRead(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), XAXIDMA_BD_CTRL_LEN_OFFSET);
            bd_sts = AxiDmaBd::XAxiDmaBdRead(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), XAXIDMA_BD_STS_OFFSET);
        }

        /* In case of Tx channel, the last BD should have EOF bit set */
        if (!(m_IsRxChannel) && !(bd_cr & XAXIDMA_BD_CTRL_TXEOF_MASK))
        {
            SPDLOG_ERROR("BdRingToHw: last BD in the set is not marked as end-of-frame");
            return false;
        }

        /* Make sure the length value in the last BD is non-zero. */
        if (AxiDmaBd::XAxiDmaBdGetLength(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), m_MaxTransferLen) == 0)
        {

            SPDLOG_ERROR("BdRingToHw: last BD in the set has zero length");
            return false;
        }

        /* The last BD should also have the completed status bit cleared */
        bd_sts &= ~XAXIDMA_BD_STS_COMPLETE_MASK;
        AxiDmaBd::XAxiDmaBdWrite(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), XAXIDMA_BD_STS_OFFSET, bd_sts);

        /* Flush the last BD so DMA core could see the updates */
        // XAXIDMA_CACHE_FLUSH(CurBdPtr);

        /* This set has completed pre-processing, adjust ring pointers and counters*/
        XAxiDmaRingSeekAhead(m_PreHead, num_bd);
        m_HwTail.SetBaseAddress(cur_bd_ptr);
        m_PreCnt -= num_bd;
        m_HwCnt += num_bd;

        int ring_index = m_RingIndex;

        /* If it is running, signal the engine to begin processing */
        if (m_RunState == AXIDMA_CHANNEL_NOT_HALTED)
        {
            if (m_Cyclic)
            {
                // m_UioDevice->Write(m_ChanBaseOffset + XAXIDMA_TDESC_OFFSET, XAxiDmaVirtToPhys(m_CyclicBd));
                m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_OFFSET, m_LastBdPhysAddr);

                if (m_Addr_ext)
                {
                    // m_UioDevice->Write(m_ChanBaseOffset + XAXIDMA_TDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_CyclicBd) >> 32) & 0xFFFFFFFF));
                    m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_MSB_OFFSET, static_cast<std::uint32_t>((m_LastBdPhysAddr >> 32) & 0xFFFFFFFF));
                }
                return true;
            }

            if (m_IsRxChannel)
            {
                if (!ring_index)
                {
                    m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_OFFSET, (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));
                    SPDLOG_DEBUG("Writing TDESC with BD addr 0x{:x}", XAxiDmaVirtToPhys(m_HwTail));
                    if (m_Addr_ext)
                    {
                        m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_HwTail) >> 32) & 0xFFFFFFFF));
                    }
                }
                else
                {
                    auto off = AxiDmaChannel::XAXIDMA_RX_TDESC0_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET;
                    m_Channel->WriteReg(off, (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));
                    SPDLOG_DEBUG("Writing TDESC[{}] with BD addr 0x{:x}", ring_index, XAxiDmaVirtToPhys(m_HwTail));
                    if (m_Addr_ext)
                    {
                        m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_MSB_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_HwTail) >> 32) & 0xFFFFFFFF));
                    }
                }
            }
            else
            {
                m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_OFFSET, (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));
                SPDLOG_DEBUG("Writing TDESC with BD addr 0x{:x}", XAxiDmaVirtToPhys(m_HwTail));
                if (m_Addr_ext)
                {
                    m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_HwTail) >> 32) & 0xFFFFFFFF));
                }
            }
        }
        return true;
    }

    bool XAxiDmaBdRing::XAxiDmaUpdateBdRingCDesc()
    {
        std::uint8_t *bd_ptr;
        int ring_index = m_RingIndex;

        /* BD list has yet to be created for this channel */
        if (m_AllCnt == 0)
        {
            SPDLOG_ERROR("UpdateBdRingCDesc: BD Ring not created yet");
            return false;
        }

        /* Do nothing if already started */
        if (m_RunState == AXIDMA_CHANNEL_NOT_HALTED)
        {
            /* Need to update tail pointer if needed (Engine is not transferring)*/
            SPDLOG_DEBUG("UpdateBdRingCDesc: channel already started, nothing to do");
            return true;
        }
        if (!XAxiDmaBdRingHwIsStarted())
        {
            /* If hardware is not running, then we need to put a valid current
             * BD pointer to the current BD register before start the hardware
             */

            /* Put a valid BD pointer in the current BD pointer register
             * So, the hardware is ready to go when tail BD pointer is updated
             */
            bd_ptr = m_BdRestart.GetBaseAddress();
            if (!AxiDmaBd::XAxiDmaBdHwCompleted(bd_ptr))
            {
                SPDLOG_DEBUG("UpdateBdRingCDesc: found uncompleted BD at addr 0x{:x}", XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)));
                if (m_IsRxChannel)
                {
                    if (!ring_index)
                    {
                        m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET, (XAxiDmaVirtToPhys(bd_ptr) & XAXIDMA_DESC_LSB_MASK));
                        SPDLOG_DEBUG("UpdateBdRingCDesc: write CDESC with BD addr 0x{:x}", XAxiDmaVirtToPhys(bd_ptr));
                        if (m_Addr_ext)
                        {
                            m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)) >> 32) & 0xFFFFFFFF));
                        }
                    }
                    else
                    {
                        auto off = AxiDmaChannel::XAXIDMA_RX_CDESC0_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET;
                        m_Channel->WriteReg(off, (XAxiDmaVirtToPhys(bd_ptr) & XAXIDMA_DESC_LSB_MASK));
                        SPDLOG_DEBUG("UpdateBdRingCDesc: write CDESC[{}] with BD addr 0x{:x}", ring_index, XAxiDmaVirtToPhys(bd_ptr));
                        if (m_Addr_ext)
                        {
                            m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_MSB_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)) >> 32) & 0xFFFFFFFF));
                        }
                    }
                }
                else
                {
                    m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET, (XAxiDmaVirtToPhys(m_BdRestart) & XAXIDMA_DESC_LSB_MASK));
                    SPDLOG_DEBUG("UpdateBdRingCDesc: write CDESC with BD addr 0x{:x}", XAxiDmaVirtToPhys(m_BdRestart));
                    if (m_Addr_ext)
                    {
                        m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_BdRestart) >> 32) & 0xFFFFFFFF));
                    }
                }
            }
            else
            {
                /* Look for an uncompleted BD*/
                while (AxiDmaBd::XAxiDmaBdHwCompleted(bd_ptr))
                {
                    bd_ptr = reinterpret_cast<std::uintptr_t>(XAxiDmaBdRingNext(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)));
                    if (bd_ptr == reinterpret_cast<std::uintptr_t>(m_BdRestart))
                    {
                        SPDLOG_ERROR("UpdateBdRingCDesc: all BDs are completed, nothing to start with");
                        return false;
                    }

                    if (!AxiDmaBd::XAxiDmaBdHwCompleted(bd_ptr))
                    {
                        if (m_IsRxChannel)
                        {
                            if (!ring_index)
                            {
                                m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET, (XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)) & XAXIDMA_DESC_LSB_MASK));
                                SPDLOG_DEBUG("UpdateBdRingCDesc: write CDESC with BD addr 0x{:x}", XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)));
                                if (m_Addr_ext)
                                {
                                    m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)) >> 32) & 0xFFFFFFFF));
                                }
                            }
                            else
                            {
                                auto off = AxiDmaChannel::XAXIDMA_RX_CDESC0_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET;
                                m_Channel->WriteReg(off, (XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)) & XAXIDMA_DESC_LSB_MASK));
                                SPDLOG_DEBUG("UpdateBdRingCDesc: write CDESC[{}] with BD addr 0x{:x}", ring_index - 1, XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)));
                                if (m_Addr_ext)
                                {
                                    m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_MSB_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)) >> 32) & 0xFFFFFFFF));
                                }
                            }
                        }
                        else
                        {
                            m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET, (XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)) & XAXIDMA_DESC_LSB_MASK));
                            SPDLOG_DEBUG("UpdateBdRingCDesc: write CDESC with BD addr 0x{:x}", XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)));
                            if (m_Addr_ext)
                            {
                                m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(reinterpret_cast<XAxiDma_Bd *>(bd_ptr)) >> 32) & 0xFFFFFFFF));
                            }
                        }
                        break;
                    }
                }
            }
        }
        return true;
    }

    bool XAxiDmaBdRing::XAxiDmaStartBdRingHw()
    {
        int ring_index = m_RingIndex;

        if (!XAxiDmaBdRingHwIsStarted())
        {
            /* Start the hardware*/
            std::uint32_t control_reg = m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CR_OFFSET);
            control_reg |= XAXIDMA_CR_RUNSTOP_MASK;
            m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_CR_OFFSET, control_reg);
        }

        if (XAxiDmaBdRingHwIsStarted())
        {
            m_RunState = AXIDMA_CHANNEL_NOT_HALTED;
            /* If there are unprocessed BDs then we want the channel to begin processing right away */
            if (m_HwCnt > 0)
            {
                // XAXIDMA_CACHE_INVALIDATE(RingPtr->HwTail);
                if (m_Cyclic)
                {
                    // m_UioDevice->Write(m_ChanBaseOffset + XAXIDMA_TDESC_OFFSET, XAxiDmaVirtToPhys(m_CyclicBd));
                    m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_OFFSET, m_LastBdPhysAddr);
                    SPDLOG_DEBUG("m_LastBdPhysAddr:0x{:X} , m_FirstBdPhysAddr:{:X} , m_FirstBdAddr:{:X} , XAxiDmaVirtToPhys(m_LastBdPhysAddr):{:X}", m_LastBdPhysAddr, m_FirstBdPhysAddr, m_FirstBdAddr, XAxiDmaVirtToPhys(m_LastBdPhysAddr));

                    if (m_Addr_ext)
                    {
                        // m_UioDevice->Write(m_ChanBaseOffset + XAXIDMA_TDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_CyclicBd) >> 32) & 0xFFFFFFFF));
                        m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_MSB_OFFSET, static_cast<std::uint32_t>((m_LastBdPhysAddr >> 32) & 0xFFFFFFFF));
                    }
                    return true;
                }

                if ((AxiDmaBd::XAxiDmaBdRead(reinterpret_cast<std::uintptr_t>(m_HwTail), XAXIDMA_BD_STS_OFFSET) & XAXIDMA_BD_STS_COMPLETE_MASK) == 0)
                {
                    if (m_IsRxChannel)
                    {
                        if (!ring_index)
                        {
                            m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_OFFSET, (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));
                            SPDLOG_DEBUG("Starting RX BD transfer at phys addr 0x{:X}", (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));

                            if (m_Addr_ext)
                            {
                                m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_HwTail) >> 32) & 0xFFFFFFFF));
                            }
                        }
                        else
                        {
                            auto off = AxiDmaChannel::XAXIDMA_RX_TDESC0_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET;
                            m_Channel->WriteReg(off, (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));
                            SPDLOG_DEBUG("Starting RX BD[{}] transfer at phys addr 0x{:X}", ring_index, (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));
                            if (m_Addr_ext)
                            {
                                m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_MSB_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_HwTail) >> 32) & 0xFFFFFFFF));
                            }
                        }
                    }
                    else
                    {
                        m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_OFFSET, (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));
                        SPDLOG_DEBUG("Starting TX BD transfer at phys addr 0x{:X}", (XAxiDmaVirtToPhys(m_HwTail) & XAXIDMA_DESC_LSB_MASK));
                        if (m_Addr_ext)
                        {
                            m_Channel->WriteReg(AxiDmaChannel::XAXIDMA_TDESC_MSB_OFFSET, static_cast<std::uint32_t>((XAxiDmaVirtToPhys(m_HwTail) >> 32) & 0xFFFFFFFF));
                        }
                    }
                }
            }
            return true;
        }
        return false;
    }

    bool XAxiDmaBdRing::XAxiDmaBdRingStart()
    {
        bool status;

        status = XAxiDmaUpdateBdRingCDesc();

        if (!status)
        {
            SPDLOG_ERROR("BdRingStart: Updating Current BD Pointer Failed\n\r");
            return status;
        }

        status = XAxiDmaStartBdRingHw();
        if (!status)
        {
            SPDLOG_ERROR("BdRingStart: Starting BD Ring Hardware Failed\n\r");
            return status;
        }

        return true;
    }

    bool XAxiDmaBdRing::XAxiDmaBdRingFromHw(int bd_limit, std::uint8_t **bd_set_ptr)
    {
        std::uint8_t *cur_bd_ptr = m_HwHead.GetBaseAddress();
        int bd_count{0};
        int bd_partial_count{0};
        std::uint32_t bd_sts{0};
        std::uint32_t bd_cr{0};

        cur_bd_ptr = m_HwHead.GetBaseAddress();

        if (m_HwCnt == 0)
        {
            *bd_set_ptr = nullptr;
            return 0;
        }

        if (bd_limit > m_HwCnt)
        {
            bd_limit = m_HwCnt;
        }

        /* Starting at HwHead, keep moving forward in the list until:
         *  - A BD is encountered with its completed bit clear in the status
         *    word which means hardware has not completed processing of that
         *    BD.
         *  - RingPtr->HwTail is reached
         *  - The number of requested BDs has been processed
         */

        while (bd_count < bd_limit)
        {
            /* Read the status */
            // XAXIDMA_CACHE_INVALIDATE(CurBdPtr);
            bd_sts = AxiDmaBd::XAxiDmaBdRead(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), XAXIDMA_BD_STS_OFFSET);
            bd_cr = AxiDmaBd::XAxiDmaBdRead(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), XAXIDMA_BD_CTRL_LEN_OFFSET);

            /* If the hardware still hasn't processed this BD then we are
             * done
             */
            if (!(bd_sts & XAXIDMA_BD_STS_COMPLETE_MASK))
            {
                break;
            }

            bd_count++;

            /* Hardware has processed this BD so check the "last" bit. If
             * it is clear, then there are more BDs for the current packet.
             * Keep a count of these partial packet BDs.
             *
             * For tx BDs, EOF bit is in the control word
             * For rx BDs, EOF bit is in the status word
             */
            if (((!(m_IsRxChannel) && (bd_cr & XAXIDMA_BD_CTRL_TXEOF_MASK)) ||
                 ((m_IsRxChannel) && (bd_sts & XAXIDMA_BD_STS_RXEOF_MASK))))
            {
                bd_partial_count = 0;
            }
            else
            {
                bd_partial_count++;
            }

            if (m_Cyclic)
            {
                bd_sts = bd_sts & ~XAXIDMA_BD_STS_COMPLETE_MASK;
                AxiDmaBd::XAxiDmaBdWrite(reinterpret_cast<std::uintptr_t>(cur_bd_ptr), XAXIDMA_BD_STS_OFFSET, bd_sts);
                // XAXIDMA_CACHE_FLUSH(cur_bd_ptr);
            }

            /* Reached the end of the work group */
            if (cur_bd_ptr == m_HwTail)
            {
                break;
            }

            /* Move on to the next BD in work group */
            cur_bd_ptr = (XAxiDma_Bd *)((void *)XAxiDmaBdRingNext(cur_bd_ptr));
        }

        /* Subtract off any partial packet BDs found */
        bd_count -= bd_partial_count;
        /* If BdCount is non-zero then BDs were found to return. Set return
         * parameters, update pointers and counters, return success
         */
        if (bd_count)
        {
            *bd_set_ptr = m_HwHead.GetBaseAddress();
            if (!m_Cyclic)
            {
                m_HwCnt -= bd_count;
                m_PostCnt += bd_count;
            }
            XAxiDmaRingSeekAhead(m_HwHead, bd_count);

            return bd_count;
        }
        else
        {
            *bd_set_ptr = nullptr;
            return 0;
        }
    }

    bool XAxiDmaBdRing::XAxiDmaBdRingHwIsStarted()
    {
        return !(m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_SR_OFFSET) & AxiDmaChannel::XAXIDMA_HALTED_MASK);
    }

    std::uint8_t *XAxiDmaBdRing::XAxiDmaVirtToPhys(const std::uint8_t *bd_ptr)
    {
        return (bd_ptr + (m_FirstBdPhysAddr - m_FirstBdAddr));
    }

    std::uint8_t *XAxiDmaBdRing::XAxiDmaVirtToPhys(std::uint8_t *bd_addr)
    {
        return bd_addr + (m_FirstBdPhysAddr - m_FirstBdAddr);
    }

    void XAxiDmaBdRing::XAxiDmaBdRingDumpRegs()
    {
        int ring_index = m_RingIndex;

        SPDLOG_INFO("===== AXI DMA BD Ring Registers Dump =====");
        SPDLOG_INFO("CR         : 0x{:08X}", m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CR_OFFSET));
        SPDLOG_INFO("SR         : 0x{:08X}", m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_SR_OFFSET));
        if (ring_index)
        {
            SPDLOG_INFO("CURDESC[{}]: 0x{:08X}", ring_index, m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_RX_CDESC0_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET));
            SPDLOG_INFO("TAILDESC[{}]: 0x{:08X}", ring_index, m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_RX_TDESC0_OFFSET + (ring_index - 1) * AxiDmaChannel::XAXIDMA_RX_NDESC_OFFSET));
        }
        else
        {
            SPDLOG_INFO("CURDESC    : 0x{:08X}", m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_CDESC_OFFSET));
            SPDLOG_INFO("TAILDESC   : 0x{:08X}", m_Channel->ReadReg(AxiDmaChannel::XAXIDMA_TDESC_OFFSET));
        }
    }
} // namespace Dma