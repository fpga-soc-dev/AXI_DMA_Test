#include "AxiDma.hpp"
#include "AxiDmaHw.hpp"
#include <spdlog/spdlog.h>

namespace Dma
{
    AxiDma::AxiDma(const std::string &uio_device, Config config) : m_UioDevice(std::make_shared<UIO>(uio_device)),
                                                                   m_Config(config),
                                                                   m_TxBdRing(m_UioDevice),
                                                                   m_RxBdRing(m_UioDevice)

    {
        if (!InitializeConfig())
        {
            SPDLOG_ERROR("Failed to initialize DMA configuration.");
        }

        if (m_Config.HasScatterGather)
        {
            SPDLOG_INFO("DMA initialized in Scatter-Gather mode.");
        }
        else
        {
            SPDLOG_INFO("DMA initialized in Simple mode.");
        }
    }

    AxiDma::~AxiDma()
    {
        Stop();
    }

    bool AxiDma::InitializeConfig()
    {
        uint32_t max_transfer_len{0};
        m_IsInitialized = false;
        m_Config.HasMm2s = HasMM2S();
        m_Config.HasS2mm = HasS2MM();
        m_Config.HasScatterGather = HasScatterGather();

        if (m_Config.Mm2SNumChannels > 1 || m_Config.S2MmNumChannels > 1)
        {
            max_transfer_len = XAXIDMA_MCHAN_MAX_TRANSFER_LEN;
        }
        else
        {
            max_transfer_len = (1U << m_Config.BufferLengthRegister) - 1;
        }

        m_TxBdRing.m_RunState = XAxiDmaBdRing::AXIDMA_CHANNEL_HALTED;
        m_TxBdRing.m_IsRxChannel = false;
        if (!m_Config.MicroDmaChannel)
        {
            m_TxBdRing.m_MaxTransferLen = max_transfer_len;
        }
        else
        {
            /* In MicroDMA mode, Maximum length that can be transferred
             * is '(Memory Data Width / 8) * Burst Size' */
            m_TxBdRing.m_MaxTransferLen = ((m_Config.Mm2SDataWidth / 8) * m_Config.Mm2SBurstSize);
        }

        m_TxBdRing.m_RingIndex = 0;

        m_RxBdRing.m_RunState = XAxiDmaBdRing::AXIDMA_CHANNEL_HALTED;
        m_RxBdRing.m_IsRxChannel = true;
        m_RxBdRing.m_RingIndex = 0;

        if (m_Config.HasMm2s)
        {
            SPDLOG_INFO("Configuring MM2S channel.");
            m_TxBdRing.m_ChanBaseOffset = XAXIDMA_TX_OFFSET;
            m_TxBdRing.m_HasStsCntrlStrm = m_Config.HasStsCntrlStrm;
            if (m_Config.AddressWidth > 32)
            {
                m_TxBdRing.m_Addr_ext = true;
            }
            else
            {
                m_TxBdRing.m_Addr_ext = false;
            }

            m_TxBdRing.m_HasDRE = m_Config.HasMm2sDRE;
            m_TxBdRing.m_DataWidth = (m_Config.Mm2SDataWidth >> 3);
        }

        if (m_Config.HasS2mm)
        {
            SPDLOG_INFO("Configuring S2MM channel.");
            m_RxBdRing.m_ChanBaseOffset = XAXIDMA_RX_OFFSET;
            m_RxBdRing.m_HasStsCntrlStrm = m_Config.HasStsCntrlStrm;
            m_RxBdRing.m_HasDRE = m_Config.HasS2mmDRE;
            m_RxBdRing.m_DataWidth = (m_Config.S2MmDataWidth >> 3);

            if (!m_Config.MicroDmaChannel)
            {
                m_RxBdRing.m_MaxTransferLen = max_transfer_len;
            }
            else
            {
                /* In MicroDMA mode, Maximum length that can be transferred is '(Memory Data Width / 8) * Burst Size' */
                m_RxBdRing.m_MaxTransferLen = ((m_Config.S2MmDataWidth / 8) * m_Config.S2MmBurstSize);
            }

            if (m_Config.AddressWidth > 32)
            {
                m_RxBdRing.m_Addr_ext = 1;
            }
            else
            {
                m_RxBdRing.m_Addr_ext = 0;
            }
        }

        XAxiDmaReset();

        int timeout = 1000;
        while (timeout--)
        {
            if (XAxiDmaResetIsDone())
            {
                break;
            }
        }

        if (timeout == 0)
        {
            SPDLOG_ERROR("DMA Reset did not complete within the timeout period.");
            return false;
        }

        if (m_Config.HasMm2s)
        {
            XAxiDmaIntrDisable(Direction::MM2S, XAXIDMA_IRQ_ALL_MASK);
            XAxiDmaIntrEnable(Direction::MM2S, XAXIDMA_IRQ_ALL_MASK);
        }
        if (m_Config.HasS2mm)
        {
            XAxiDmaIntrDisable(Direction::S2MM, XAXIDMA_IRQ_ALL_MASK);
            XAxiDmaIntrEnable(Direction::S2MM, XAXIDMA_IRQ_ALL_MASK);
        }

        m_IsInitialized = true;
        return true;
    }

    bool AxiDma::HasMM2S()
    {
        return (m_UioDevice->Read(XAXIDMA_TX_OFFSET + XAXIDMA_CR_OFFSET) & XAXIDMA_CR_RESERVED) != 0;
    }

    bool AxiDma::HasS2MM()
    {
        return (m_UioDevice->Read(XAXIDMA_RX_OFFSET + XAXIDMA_CR_OFFSET) & XAXIDMA_CR_RESERVED) != 0;
    }

    bool AxiDma::HasScatterGather()
    {
        return (m_UioDevice->Read(XAXIDMA_TX_OFFSET + XAXIDMA_SR_OFFSET) & XAXIDMA_SG_INCLUDE_MASK) != 0 ||
               (m_UioDevice->Read(XAXIDMA_RX_OFFSET + XAXIDMA_SR_OFFSET) & XAXIDMA_SG_INCLUDE_MASK) != 0;
    }

    void AxiDma::XAxiDmaIntrDisable(Direction direction, std::uint32_t mask)
    {
        if (direction == Direction::MM2S)
        {
            auto reg = m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
            reg &= ~(mask & XAXIDMA_IRQ_ALL_MASK);
            m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, reg);
        }
        else if (direction == Direction::S2MM)
        {
            auto reg = m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
            reg &= ~(mask & XAXIDMA_IRQ_ALL_MASK);
            m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, reg);
        }
    }

    bool AxiDma::XAxiDmaIsStarted(Direction direction)
    {
        if (direction == Direction::MM2S)
        {
            return !(m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK);
        }
        else if (direction == Direction::S2MM)
        {
            return !(m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK);
        }
        return false;
    }

    void AxiDma::XAxiDmaIntrEnable(Direction direction, std::uint32_t mask)
    {
        if (direction == Direction::MM2S)
        {
            auto reg = m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
            reg |= (mask & XAXIDMA_IRQ_ALL_MASK);
            m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, reg);
        }
        else if (direction == Direction::S2MM)
        {
            auto reg = m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
            reg |= (mask & XAXIDMA_IRQ_ALL_MASK);
            m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, reg);
        }
    }

    std::uint32_t AxiDma::XAxiDmaIntrGetIrq(Direction direction)
    {
        if (direction == Direction::MM2S)
        {
            return m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET) & XAXIDMA_IRQ_ALL_MASK;
        }
        else if (direction == Direction::S2MM)
        {
            return m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET) & XAXIDMA_IRQ_ALL_MASK;
        }
    }

    bool AxiDma::XAxiDmaSimpleTransfer(Direction direction, std::uint64_t buffer_address, std::uint32_t length)
    {
        std::uint32_t word;

        if (m_Config.HasScatterGather)
        {
            SPDLOG_ERROR("Simple transfer is not supported in Scatter-Gather mode.");
            return false;
        }

        if (direction == Direction::MM2S)
        {
            if (length > m_TxBdRing.m_MaxTransferLen)
            {
                SPDLOG_ERROR("Transfer length {} exceeds maximum allowed length {} for MM2S.", length, m_TxBdRing.m_MaxTransferLen);
                return false;
            }

            if (!m_Config.HasMm2s)
            {
                SPDLOG_ERROR("Simple transfer is not supported for MM2S.");
                return false;
            }
            /* If the engine is doing transfer, cannot submit */
            if (!(m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK))
            {
                if (XAxiDmaBusy(direction))
                {
                    SPDLOG_ERROR("DMA MM2S engine is currently running. Cannot submit new transfer.");
                    return false;
                }
            }

            if (!m_Config.MicroDmaChannel)
            {
                word = static_cast<uint32_t>((m_TxBdRing.m_DataWidth) - 1);
            }
            else
            {
                word = XAXIDMA_MICROMODE_MIN_BUF_ALIGN;
            }

            if ((buffer_address & word) != 0)
            {
                if (!m_Config.HasMm2sDRE)
                {
                    SPDLOG_ERROR("Unaligned transfer without DRE %x\r\n", buffer_address);
                    return false;
                }
            }
        }
        else if (direction == Direction::S2MM)
        {
            if (length > m_RxBdRing.m_MaxTransferLen)
            {
                SPDLOG_ERROR("Transfer length {} exceeds maximum allowed length {} for S2MM.", length, m_RxBdRing.m_MaxTransferLen);
                return false;
            }

            if (!m_Config.HasS2mm)
            {
                SPDLOG_ERROR("Simple transfer is not supported for S2MM.");
                return false;
            }
            /* If the engine is doing transfer, cannot submit */
            if (!(m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK))
            {
                if (XAxiDmaBusy(direction))
                {
                    SPDLOG_ERROR("DMA S2MM engine is currently running. Cannot submit new transfer.");
                    return false;
                }
            }

            if (!m_Config.MicroDmaChannel)
            {
                word = static_cast<uint32_t>((m_RxBdRing.m_DataWidth) - 1);
            }
            else
            {
                word = XAXIDMA_MICROMODE_MIN_BUF_ALIGN;
            }

            if ((buffer_address & word) != 0)
            {
                if (!m_Config.HasS2mmDRE)
                {
                    SPDLOG_ERROR("Unaligned transfer without DRE %x\r\n", buffer_address);
                    return false;
                }
            }
        }

        XAxiDmaSetBufferAddress(direction, buffer_address);
        XAxiDmaStart(direction, true);
        XAxiDmaLength(direction, length);

        return true;
    }

    bool AxiDma::XAxiDmaBusy(Direction direction)
    {
        if (direction == Direction::MM2S)
        {
            return (m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET) & XAXIDMA_IDLE_MASK) ? false : true;
        }
        else if (direction == Direction::S2MM)
        {
            return (m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET) & XAXIDMA_IDLE_MASK) ? false : true;
        }
    }

    AxiDma::StatusBits AxiDma::XAxiDmaStatusRead(Direction direction)
    {
        std::uint32_t status_reg;
        if (direction == Direction::MM2S)
        {
            status_reg = m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET);
        }
        else if (direction == Direction::S2MM)
        {
            status_reg = m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET);
        }

        StatusBits status;
        status.halted = status_reg & 0x1;
        status.idle = (status_reg >> 1) & 0x1;
        status.sg_included = (status_reg >> 3) & 0x1;
        status.dma_internal_error = (status_reg >> 4) & 0x1;
        status.dma_slave_error = (status_reg >> 5) & 0x1;
        status.dma_decode_error = (status_reg >> 6) & 0x1;
        status.sg_internal_error = (status_reg >> 8) & 0x1;
        status.sg_slave_error = (status_reg >> 9) & 0x1;
        status.sg_decode_error = (status_reg >> 10) & 0x1;
        status.ioc_irq = (status_reg >> 12) & 0x1;
        status.dly_irq = (status_reg >> 13) & 0x1;
        status.err_irq = (status_reg >> 14) & 0x1;
        status.irq_threshold_sts = (status_reg >> 16) & 0xFF;
        status.irq_delay_sts = (status_reg >> 24) & 0xFF;

        return status;
    }

    AxiDma::ControlBits AxiDma::XAxiDmaControlRead(Direction direction)
    {
        std::uint32_t control_reg;
        if (direction == Direction::MM2S)
        {
            control_reg = m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
        }
        else if (direction == Direction::S2MM)
        {
            control_reg = m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
        }

        ControlBits control;
        control.rs = control_reg & 0x1;
        control.reset = (control_reg >> 2) & 0x1;
        control.keyhole = (control_reg >> 3) & 0x1;
        control.cyclic_bd_enable = (control_reg >> 4) & 0x1;
        control.ioc_irq_enable = (control_reg >> 12) & 0x1;
        control.dly_irq_enable = (control_reg >> 13) & 0x1;
        control.err_irq_enable = (control_reg >> 14) & 0x1;
        control.irq_threshold = (control_reg >> 16) & 0xFF;
        control.irq_delay = (control_reg >> 24) & 0xFF;

        return control;
    }

    void AxiDma::XAxiDmaReset()
    {
        if (m_Config.HasMm2s)
        {

            /* Save the locations of current BDs both rings are working on
             * before the reset so later we can resume the rings smoothly.
             */
            if (m_Config.HasScatterGather)
            {
                m_TxBdRing.XAxiDmaBdRingSnapShotCurrBd();
                m_RxBdRing.XAxiDmaBdRingSnapShotCurrBd();
            }
        }

        // Reset MM2S
        if (m_Config.HasMm2s)
        {
            m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, XAXIDMA_CR_RESET_MASK);
            m_TxBdRing.m_RunState = XAxiDmaBdRing::AXIDMA_CHANNEL_HALTED;
        }

        // Reset S2MM
        if (m_Config.HasS2mm)
        {
            m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, XAXIDMA_CR_RESET_MASK);
            m_RxBdRing.m_RunState = XAxiDmaBdRing::AXIDMA_CHANNEL_HALTED;
        }
    }

    bool AxiDma::XAxiDmaResetIsDone()
    {
        if (m_Config.HasMm2s)
        {
            auto reg_value = m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
            /* Reset is done when the reset bit is low */
            if (reg_value & XAXIDMA_CR_RESET_MASK)
            {
                return false;
            }
        }

        if (m_Config.HasS2mm)
        {
            auto reg_value = m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
            /* Reset is done when the reset bit is low */
            if (reg_value & XAXIDMA_CR_RESET_MASK)
            {
                return false;
            }
        }
        return true;
    }

    void AxiDma::XAxiDmaStart(Direction direction, bool start)
    {
        if (direction == Direction::MM2S)
        {
            uint32_t control_reg = m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
            if (start)
            {
                control_reg |= XAXIDMA_CR_RUNSTOP_MASK; // Set the run/stop bit
            }
            else
            {
                control_reg &= ~XAXIDMA_CR_RUNSTOP_MASK; // Clear the run/stop bit
            }
            m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, control_reg);
        }

        else if (direction == Direction::S2MM)
        {
            uint32_t control_reg = m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
            if (start)
            {
                control_reg |= XAXIDMA_CR_RUNSTOP_MASK; // Set the run/stop bit
            }
            else
            {
                control_reg &= ~XAXIDMA_CR_RUNSTOP_MASK; // Clear the run/stop bit
            }
            m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, control_reg);
        }
    }

    void AxiDma::XAxiDmaSetBufferAddress(Direction direction, std::uint64_t buffer_address)
    {
        if (direction == Direction::MM2S)
        {
            m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_SRCADDR_OFFSET, static_cast<uint32_t>(buffer_address & 0xFFFFFFFF));
            if (m_Config.AddressWidth > 32)
            {
                m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_SRCADDR_MSB_OFFSET, static_cast<uint32_t>((buffer_address >> 32) & 0xFFFFFFFF));
            }
        }
        else if (direction == Direction::S2MM)
        {
            m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_DESTADDR_OFFSET, static_cast<uint32_t>(buffer_address & 0xFFFFFFFF));
            if (m_Config.AddressWidth > 32)
            {
                m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_DESTADDR_MSB_OFFSET, static_cast<uint32_t>((buffer_address >> 32) & 0xFFFFFFFF));
            }
        }
    }

    void AxiDma::XAxiDmaLength(Direction direction, uint32_t length)
    {
        if (direction == Direction::MM2S)
        {
            m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_BUFFLEN_OFFSET, length);
        }
        else if (direction == Direction::S2MM)
        {
            m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_BUFFLEN_OFFSET, length);
        }
    }

    void AxiDma::XAxiDmaWaitSync(Direction direction)
    {
        StatusBits status = XAxiDmaStatusRead(direction);
        while (!status.idle)
        {
            if (status.dma_internal_error || status.dma_slave_error || status.dma_decode_error)
            {
                SPDLOG_ERROR("MM2S encountered an error during transfer");
                break;
            }
            status = XAxiDmaStatusRead(direction);
        }
        SPDLOG_DEBUG("MM2S Transfer complete");
    }

    void AxiDma::XAxiDmaClearInterrupts(Direction direction)
    {
        auto reg_addr = direction == Direction::MM2S ? m_TxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET
                                                     : m_RxBdRing.m_ChanBaseOffset + XAXIDMA_SR_OFFSET;

        auto reg_value = m_UioDevice->Read(reg_addr);
        reg_value |= XAXIDMA_IRQ_ALL_MASK;
        m_UioDevice->Write(reg_addr, reg_value);
    }

    void AxiDma::AttachInterruptHandler(Direction direction, const std::function<void()> &callback, bool queue_enabled)
    {

        if (direction == Direction::MM2S)
        {
            m_Mm2sCallback = callback;
            // m_UioDevice->InterruptAttach([this]()
            //                              { OnMm2sInterrupt(); },
            //                              queue_enabled);
        }
        else if (direction == Direction::S2MM)
        {
            m_S2mmCallback = callback;
            m_UioDevice->InterruptAttach([this]()
                                         { OnS2mmInterrupt(); },
                                         queue_enabled);
        }
    }

    void AxiDma::DetachInterruptHandler()
    {
        m_UioDevice->InterruptDetach();
        m_S2mmCallback = nullptr;
        m_Mm2sCallback = nullptr;
    }

    void AxiDma::OnS2mmInterrupt()
    {
        auto status = XAxiDmaStatusRead(Direction::S2MM);

        if (status.ioc_irq || status.err_irq || status.dly_irq)
        {
            XAxiDmaClearInterrupts(Direction::S2MM);

            if (status.err_irq)
            {
                SPDLOG_ERROR("S2MM DMA Error interrupt - Details:");
                SPDLOG_ERROR("  DMA Internal Error: {}", status.dma_internal_error);
                SPDLOG_ERROR("  DMA Slave Error: {}", status.dma_slave_error);
                SPDLOG_ERROR("  DMA Decode Error: {}", status.dma_decode_error);
                XAxiDmaPrintStatus(Direction::S2MM);
                return;
            }

            if (m_S2mmCallback)
            {
                m_S2mmCallback();
            }
        }
    }

    void AxiDma::XAxiDmaPrintStatus(Direction direction)

    {
        StatusBits status = XAxiDmaStatusRead(direction);
        SPDLOG_DEBUG("{} Status:", direction == Direction::MM2S ? "MM2S" : "S2MM");
        SPDLOG_DEBUG("  halted={}", status.halted);
        SPDLOG_DEBUG("  idle={}", status.idle);
        SPDLOG_DEBUG("  sg_included={}", status.sg_included);
        SPDLOG_DEBUG("  dma_internal_error={}", status.dma_internal_error);
        SPDLOG_DEBUG("  dma_slave_error={}", status.dma_slave_error);
        SPDLOG_DEBUG("  dma_decode_error={}", status.dma_decode_error);
        SPDLOG_DEBUG("  sg_internal_error={}", status.sg_internal_error);
        SPDLOG_DEBUG("  sg_slave_error={}", status.sg_slave_error);
        SPDLOG_DEBUG("  sg_decode_error={}", status.sg_decode_error);
        SPDLOG_DEBUG("  ioc_irq={}", status.ioc_irq);
        SPDLOG_DEBUG("  dly_irq={}", status.dly_irq);
        SPDLOG_DEBUG("  err_irq={}", status.err_irq);
        SPDLOG_DEBUG("  irq_threshold_sts={}", status.irq_threshold_sts);
        SPDLOG_DEBUG("  irq_delay_sts={}", status.irq_delay_sts);
    }

    void AxiDma::XAxiDmaPrintControl(Direction direction)
    {
        ControlBits control_reg = XAxiDmaControlRead(direction);
        SPDLOG_DEBUG("{} Control Register Bits:", direction == Direction::MM2S ? "MM2S" : "S2MM");
        SPDLOG_DEBUG("  rs={}", control_reg.rs);
        SPDLOG_DEBUG("  reset={}", control_reg.reset);
        SPDLOG_DEBUG("  keyhole={}", control_reg.keyhole);
        SPDLOG_DEBUG("  cyclic_bd_enable={}", control_reg.cyclic_bd_enable);
        SPDLOG_DEBUG("  ioc_irq_enable={}", control_reg.ioc_irq_enable);
        SPDLOG_DEBUG("  dly_irq_enable={}", control_reg.dly_irq_enable);
        SPDLOG_DEBUG("  err_irq_enable={}", control_reg.err_irq_enable);
        SPDLOG_DEBUG("  irq_threshold={}", control_reg.irq_threshold);
        SPDLOG_DEBUG("  irq_delay={}", control_reg.irq_delay);
    }

    void AxiDma::Stop()
    {
        if (m_Config.HasMm2s)
        {
            XAxiDmaStart(Direction::MM2S, false);
            int timeout = 1000;
            while (timeout-- && XAxiDmaBusy(Direction::MM2S))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }

            XAxiDmaClearInterrupts(Direction::MM2S);
            XAxiDmaIntrDisable(Direction::MM2S, XAXIDMA_IRQ_ALL_MASK);
        }

        if (m_Config.HasS2mm)
        {
            XAxiDmaStart(Direction::S2MM, false);
            int timeout = 1000;
            while (timeout-- && XAxiDmaBusy(Direction::S2MM))
            {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
            XAxiDmaClearInterrupts(Direction::S2MM);
            XAxiDmaIntrDisable(Direction::S2MM, XAXIDMA_IRQ_ALL_MASK);
        }
    }

    bool AxiDma::XAxiDmaSelectCyclicMode(Direction direction, bool enable)
    {
        uint32_t control_reg{0};

        if (direction == Direction::MM2S)
        {
            control_reg = m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
        }
        else
        {
            control_reg = m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
        }

        if (enable)
        {
            control_reg |= XAXIDMA_CR_CYCLIC_MASK;
        }
        else
        {
            control_reg &= ~XAXIDMA_CR_CYCLIC_MASK;
        }

        if (direction == Direction::MM2S)
        {
            m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, control_reg);
        }
        else
        {
            m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, control_reg);
        }

        return true;
    }

    bool AxiDma::XAxiDmaSelectKeyHoleMode(Direction direction, bool enable)
    {
        uint32_t control_reg{0};

        if (direction == Direction::MM2S)
        {
            control_reg = m_UioDevice->Read(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
        }
        else
        {
            control_reg = m_UioDevice->Read(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET);
        }

        if (enable)
        {
            control_reg |= XAXIDMA_CR_KEYHOLE_MASK;
        }
        else
        {
            control_reg &= ~XAXIDMA_CR_KEYHOLE_MASK;
        }

        if (direction == Direction::MM2S)
        {
            m_UioDevice->Write(m_TxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, control_reg);
        }
        else
        {
            m_UioDevice->Write(m_RxBdRing.m_ChanBaseOffset + XAXIDMA_CR_OFFSET, control_reg);
        }

        return true;
    }

    bool AxiDma::XAxiDmaCyclicTransfer(Direction direction, const UDmaBuffer &buffers)
    {
        auto bd_ring_buf_length = buffers.GetBufferSize("udmabuf0");
        auto bdring_buf_phyaddr = buffers.GetPhysicalAddress("udmabuf0");
        auto bdring_buf_virtaddr = reinterpret_cast<std::uintptr_t>(buffers.BufferPtr("udmabuf0"));

        auto data_buffer_length = buffers.GetBufferSize("udmabuf1");
        auto data_buffer_phyaddr = buffers.GetPhysicalAddress("udmabuf1");
        auto data_buffer_virtaddr = reinterpret_cast<std::uintptr_t>(buffers.BufferPtr("udmabuf1"));

        if (!m_Config.HasScatterGather)
        {
            SPDLOG_ERROR("Cyclic transfer is not supported in Scatter-Gather mode.");
            return false;
        }

        if (direction == Direction::MM2S)
        {
        }
        else if (direction == Direction::S2MM)
        {
            bool status;
            XAxiDma_Bd bd_template;
            XAxiDma_Bd *bd_ptr;
            XAxiDma_Bd *bd_curr_ptr;
            int bd_count;
            int free_bd_count;
            std::uintptr_t rx_buffer_address;

            /* Disable all RX interrupts before RxBD space setup */
            XAxiDmaIntrDisable(direction, XAXIDMA_IRQ_ALL_MASK);

            /* Setup Rx BD space */
            bd_count = m_RxBdRing.XAxiDmaBdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, bd_ring_buf_length);
            SPDLOG_DEBUG("Cyclic RX BD count calculated: {}", bd_count);

            status = m_RxBdRing.XAxiDmaBdRingCreate(bdring_buf_phyaddr, bdring_buf_virtaddr, XAXIDMA_BD_MINIMUM_ALIGNMENT, bd_count);
            if (!status)
            {
                SPDLOG_ERROR("Failed to create BD Ring for cyclic transfer.");
                return false;
            }

            /*Setup a BD template for the Rx channel. Then copy it to every RX BD.*/
            AxiDmaBd::XAxiDmaBdClear(reinterpret_cast<std::uintptr_t>(bd_template));

            status = m_RxBdRing.XAxiDmaBdRingClone(&bd_template);
            if (!status)
            {
                SPDLOG_ERROR("Rx bd clone failed.");
                return false;
            }

            free_bd_count = m_RxBdRing.m_FreeCnt;
            status = m_RxBdRing.XAxiDmaBdRingAlloc(free_bd_count, &bd_ptr);
            if (!status)
            {
                SPDLOG_ERROR("Rx bd allocation failed.");
                return false;
            }

            bd_curr_ptr = bd_ptr;
            rx_buffer_address = data_buffer_phyaddr;

            for (int index = 0; index < free_bd_count; ++index)
            {
                status = AxiDmaBd::XAxiDmaBdSetBufAddr(reinterpret_cast<std::uintptr_t>(bd_curr_ptr), rx_buffer_address);
                if (!status)
                {
                    SPDLOG_ERROR("Rx set buffer address failed for buf_addr={:08x} bd_ptr={:08x} status={}",
                                 (unsigned int)rx_buffer_address,
                                 (uintptr_t)bd_curr_ptr, status);
                    return false;
                }

                status = AxiDmaBd::XAxiDmaBdSetLength(reinterpret_cast<std::uintptr_t>(bd_curr_ptr), bd_ring_buf_length, m_RxBdRing.m_MaxTransferLen);
                if (!status)
                {
                    SPDLOG_ERROR("Rx set length failed for length={} bd_ptr={:08x} status={}",
                                 bd_ring_buf_length,
                                 (uintptr_t)bd_curr_ptr, status);
                    return false;
                }

                /* Receive BDs do not need to set anything for the control
                 * The hardware will set the SOF/EOF bits per stream status
                 */
                AxiDmaBd::XAxiDmaBdSetCtrl(reinterpret_cast<std::uintptr_t>(bd_curr_ptr), 0);
                AxiDmaBd::XAxiDmaBdSetId(reinterpret_cast<std::uintptr_t>(bd_curr_ptr), rx_buffer_address);

                rx_buffer_address += data_buffer_length;
                bd_curr_ptr = m_RxBdRing.XAxiDmaBdRingNext(reinterpret_cast<const XAxiDma_Bd *>(bd_curr_ptr));
            }

            status = m_RxBdRing.XAxiDmaBdRingSetCoalesce(16, 100);
            if (!status)
            {
                SPDLOG_ERROR("Failed to set coalesce for Rx BD ring.");
                return false;
            }

            status = m_RxBdRing.XAxiDmaBdRingToHw(free_bd_count, bd_ptr);
            if (!status)
            {
                SPDLOG_ERROR("Failed to submit Rx BD ring to hardware.");
                return false;
            }

            /* Enable all interrupts */
            XAxiDmaIntrEnable(direction, XAXIDMA_IRQ_ALL_MASK);
            /* Enable Cyclic DMA mode */
            m_RxBdRing.m_Cyclic = true;
            XAxiDmaSelectCyclicMode(direction, true);

            /* Start RX DMA channel */
            status = m_RxBdRing.XAxiDmaBdRingStart();
            if (!status)
            {
                SPDLOG_ERROR("Failed to start Rx BD ring.");
                return false;
            }
        }

        return true;
    }

} // namespace Dma