#include "AxiDma.hpp"
#include <spdlog/spdlog.h>

namespace Dma
{
    AxiDma::AxiDma(const std::string &uio_device) : m_UioDevice(std::make_unique<UIO>(uio_device))
    {
    }

    AxiDma::~AxiDma()
    {
    }

    AxiDma::StatusBits AxiDma::ReadMm2sStatus()
    {
        uint32_t status_reg = m_UioDevice->Read(MM2S_STATUS_REGISTER);
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

    AxiDma::StatusBits AxiDma::ReadS2mmStatus()
    {
        uint32_t status_reg = m_UioDevice->Read(S2MM_STATUS_REGISTER);
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

    AxiDma::ControlBits AxiDma::ReadMm2sControl()
    {
        uint32_t control_reg = m_UioDevice->Read(MM2S_CONTROL_REGISTER);
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

    AxiDma::ControlBits AxiDma::ReadS2mmControl()
    {
        uint32_t control_reg = m_UioDevice->Read(S2MM_CONTROL_REGISTER);
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

    void AxiDma::ResetMm2s()
    {
        uint32_t control_reg = m_UioDevice->Read(MM2S_CONTROL_REGISTER);
        control_reg |= 0x4; // Set the reset bit
        m_UioDevice->Write(MM2S_CONTROL_REGISTER, control_reg);

        do
        {
            control_reg = m_UioDevice->Read(MM2S_CONTROL_REGISTER);
        } while (control_reg & 0x4);
        SPDLOG_DEBUG("MM2S reset complete");
    }

    void AxiDma::ResetS2mm()
    {
        uint32_t control_reg = m_UioDevice->Read(S2MM_CONTROL_REGISTER);
        control_reg |= 0x4; // Set the reset bit
        m_UioDevice->Write(S2MM_CONTROL_REGISTER, control_reg);

        do
        {
            control_reg = m_UioDevice->Read(S2MM_CONTROL_REGISTER);
        } while (control_reg & 0x4);
        SPDLOG_DEBUG("S2MM reset complete");
    }

    void AxiDma::HaltMm2s()
    {
        uint32_t control_reg = m_UioDevice->Read(MM2S_CONTROL_REGISTER);
        control_reg &= ~0x1; // Clear the run/stop bit
        m_UioDevice->Write(MM2S_CONTROL_REGISTER, control_reg);
        SPDLOG_DEBUG("MM2S halted");
    }

    void AxiDma::HaltS2mm()
    {
        uint32_t control_reg = m_UioDevice->Read(S2MM_CONTROL_REGISTER);
        control_reg &= ~0x1; // Clear the run/stop bit
        m_UioDevice->Write(S2MM_CONTROL_REGISTER, control_reg);
        SPDLOG_DEBUG("S2MM halted");
    }

    void AxiDma::SetSourceAddress(uint64_t address)
    {
        uint32_t address_low = static_cast<uint32_t>(address & 0xFFFFFFFF);
        uint32_t address_high = static_cast<uint32_t>((address >> 32) & 0xFFFFFFFF);
        m_UioDevice->Write(MM2S_START_ADDRESS, address_low);
        m_UioDevice->Write(MM2S_START_ADDRESS_MSB, address_high);
        SPDLOG_DEBUG("MM2S source address set to 0x{:08X}{:08X}", address_high, address_low);
    }

    void AxiDma::SetDestinationAddress(uint64_t address)
    {
        uint32_t address_low = static_cast<uint32_t>(address & 0xFFFFFFFF);
        uint32_t address_high = static_cast<uint32_t>((address >> 32) & 0xFFFFFFFF);
        m_UioDevice->Write(S2MM_START_ADDRESS, address_low);
        m_UioDevice->Write(S2MM_START_ADDRESS_MSB, address_high);
        SPDLOG_DEBUG("S2MM destination address set to 0x{:08X}{:08X}", address_high, address_low);
    }

    void AxiDma::SetMm2sLength(uint32_t length)
    {
        m_UioDevice->Write(MM2S_LENGTH, length);
        SPDLOG_DEBUG("MM2S length set to {}", length);
    }

    void AxiDma::SetS2mmLength(uint32_t length)
    {
        m_UioDevice->Write(S2MM_LENGTH, length);
        SPDLOG_DEBUG("S2MM length set to {}", length);
    }

    void AxiDma::SetAllInterruptsMaskedMm2s()
    {
        uint32_t control_reg = m_UioDevice->Read(MM2S_CONTROL_REGISTER);
        control_reg |= 0xF001;
        m_UioDevice->Write(MM2S_CONTROL_REGISTER, control_reg);
        SPDLOG_DEBUG("MM2S all interrupts masked");
    }

    void AxiDma::SetAllInterruptsMaskedS2mm()
    {
        uint32_t control_reg = m_UioDevice->Read(S2MM_CONTROL_REGISTER);
        control_reg |= 0xF001; // Mask all interrupts
        m_UioDevice->Write(S2MM_CONTROL_REGISTER, control_reg);
        SPDLOG_DEBUG("S2MM all interrupts masked");
    }

    void AxiDma::WaitS2mmSync()
    {
        StatusBits status = ReadS2mmStatus();
        while (!status.idle)
        {
            if (status.dma_internal_error || status.dma_slave_error || status.dma_decode_error)
            {
                SPDLOG_ERROR("S2MM encountered an error during transfer");
                break;
            }
            status = ReadS2mmStatus();
        }
        SPDLOG_DEBUG("S2MM Transfer complete");
    }

    void AxiDma::WaitMm2sSync()
    {
        StatusBits status = ReadMm2sStatus();
        while (!status.idle)
        {
            if (status.dma_internal_error || status.dma_slave_error || status.dma_decode_error)
            {
                SPDLOG_ERROR("MM2S encountered an error during transfer");
                break;
            }
            status = ReadMm2sStatus();
        }
        SPDLOG_DEBUG("MM2S Transfer complete");
    }

    void AxiDma::ClearAllInterruptsS2mm()
    {
        auto reg_value = m_UioDevice->Read(S2MM_STATUS_REGISTER);
        reg_value |= (1u << 12) | (1u << 13) | (1u << 14);
        m_UioDevice->Write(S2MM_STATUS_REGISTER, reg_value);
        SPDLOG_DEBUG("S2MM all interrupts cleared");
    }

    void AxiDma::ClearAllInterruptsMm2s()
    {
        auto reg_value = m_UioDevice->Read(MM2S_STATUS_REGISTER);
        reg_value |= (1u << 12) | (1u << 13) | (1u << 14);
        m_UioDevice->Write(MM2S_STATUS_REGISTER, reg_value);
        SPDLOG_DEBUG("MM2S all interrupts cleared");
    }

    void AxiDma::StartS2mm(uint64_t address, uint32_t length)
    {
        ResetS2mm();
        HaltS2mm();
        SetDestinationAddress(address);
        SetAllInterruptsMaskedS2mm();
        SetS2mmLength(length);
        WaitS2mmSync();
        SPDLOG_DEBUG("S2MM prepared with address 0x{:08X}{:08X} and length {}", static_cast<uint32_t>((address >> 32) & 0xFFFFFFFF), static_cast<uint32_t>(address & 0xFFFFFFFF), length);
    }

    void AxiDma::StartMm2s(uint64_t address, uint32_t length)
    {
        ResetMm2s();
        HaltMm2s();
        SetSourceAddress(address);
        SetAllInterruptsMaskedMm2s();
        SetMm2sLength(length);
        WaitMm2sSync();
        SPDLOG_DEBUG("MM2S prepared with address 0x{:08X}{:08X} and length {}", static_cast<uint32_t>((address >> 32) & 0xFFFFFFFF), static_cast<uint32_t>(address & 0xFFFFFFFF), length);
    }

    void AxiDma::AttachS2mmInterruptHandler(const std::function<void()> &callback, bool queue_enabled)
    {
        m_S2mmCallback = callback;
        m_UioDevice->InterruptAttach([this]()
                                     { OnInterruptS2mm(); },
                                     queue_enabled);
    }

    void AxiDma::DetachS2mmInterruptHandler()
    {
        m_UioDevice->InterruptDetach();
        m_S2mmCallback = nullptr;
    }

    void AxiDma::OnInterruptS2mm()
    {
        auto s2mm_status = ReadS2mmStatus();

        if (m_S2mmCallback)
        {
            m_S2mmCallback();
        }

        if (s2mm_status.ioc_irq || s2mm_status.err_irq || s2mm_status.dly_irq)
        {
            ClearAllInterruptsS2mm();
        }

        if (m_S2mmCallback)
        {
            m_S2mmCallback();
        }
    }

    void AxiDma::PrintMm2sStatus()
    {
        StatusBits status = ReadMm2sStatus();
        SPDLOG_DEBUG("MM2S Status:");
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

    void AxiDma::PrintS2mmStatus()
    {
        StatusBits status = ReadS2mmStatus();
        SPDLOG_DEBUG("S2MM Status:");
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

    void AxiDma::PrintMm2sControl()
    {
        ControlBits control_reg = ReadMm2sControl();
        SPDLOG_DEBUG("MM2S Control Register Bits:");
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

    void AxiDma::PrintS2mmControl()
    {
        ControlBits control_reg = ReadS2mmControl();
        SPDLOG_DEBUG("S2MM Control Register Bits:");
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
        HaltMm2s();
        HaltS2mm();
        SPDLOG_DEBUG("AxiDma MM2S and S2MM channels stopped");
    }

} // namespace Dma