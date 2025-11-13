#include "AxiDmaChannel.hpp"
#include <spdlog/spdlog.h>

namespace Dma
{

  AxiDmaChannel::AxiDmaChannel(std::shared_ptr<UIO> uio, Direction direction)
      : m_Uio(uio),
        m_Direction(direction),
        m_BaseAddress(direction == Direction::MM2S ? XAXIDMA_TX_OFFSET : XAXIDMA_RX_OFFSET)
  {
  }

  bool AxiDmaChannel::XAxiDmaReset()
  {
    auto control_reg = ReadReg(XAXIDMA_CR_OFFSET);
    control_reg |= XAXIDMA_CR_RESET_MASK;
    WriteReg(XAXIDMA_CR_OFFSET, control_reg);

    for (int i = 0; i < 1000; ++i)
    {
      auto r = ReadReg(XAXIDMA_CR_OFFSET);
      if ((r & XAXIDMA_CR_RESET_MASK) == 0)
      {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    SPDLOG_ERROR("XAxiDmaReset: Timeout waiting for reset to complete");
    return false;
  }

  void AxiDmaChannel::XAxiDmaStart()
  {
    auto control_reg = ReadReg(XAXIDMA_CR_OFFSET);
    control_reg |= XAXIDMA_CR_RUNSTOP_MASK; // Set the run/stop bit
    WriteReg(XAXIDMA_CR_OFFSET, control_reg);
    for (int i = 0; i < 1000; ++i)
    {
      auto r = ReadReg(XAXIDMA_CR_OFFSET);
      if ((r & XAXIDMA_CR_RUNSTOP_MASK) != 0)
      {
        return;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    SPDLOG_ERROR("XAxiDmaStart: Timeout waiting for channel to start");
  }

  void AxiDmaChannel::XAxiDmaStop()
  {
    auto control_reg = ReadReg(XAXIDMA_CR_OFFSET);
    control_reg &= ~XAXIDMA_CR_RUNSTOP_MASK; // Clear the run/stop bit
    WriteReg(XAXIDMA_CR_OFFSET, control_reg);
    for (int i = 0; i < 1000; ++i)
    {
      auto r = ReadReg(XAXIDMA_CR_OFFSET);
      if ((r & XAXIDMA_CR_RUNSTOP_MASK) == 0)
      {
        return;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    SPDLOG_ERROR("XAxiDmaStop: Timeout waiting for channel to stop");
  }

  void AxiDmaChannel::XAxiDmaSetCurrentDescriptor(std::uint64_t desc_addr)
  {
    WriteReg(XAXIDMA_CDESC_OFFSET, desc_addr & 0xFFFFFFFF);
    WriteReg(XAXIDMA_CDESC_MSB_OFFSET, desc_addr >> 32);
  }

  std::uint32_t AxiDmaChannel::XAxiDmaGetCurrentDescriptorLow() const
  {
    return ReadReg(XAXIDMA_CDESC_OFFSET);
  }

  std::uint32_t AxiDmaChannel::XAxiDmaGetCurrentDescriptorHigh() const
  {
    return ReadReg(XAXIDMA_CDESC_MSB_OFFSET);
  }

  void AxiDmaChannel::XAxiDmaSetTailDescriptor(std::uint64_t desc_addr)
  {
    WriteReg(XAXIDMA_TDESC_OFFSET, desc_addr & 0xFFFFFFFF);
    WriteReg(XAXIDMA_TDESC_MSB_OFFSET, desc_addr >> 32);
  }

  std::uint32_t AxiDmaChannel::XAxiDmaGetTailDescriptorLow() const
  {
    return ReadReg(XAXIDMA_TDESC_OFFSET);
  }

  std::uint32_t AxiDmaChannel::XAxiDmaGetTailDescriptorHigh() const
  {
    return ReadReg(XAXIDMA_TDESC_MSB_OFFSET);
  }

  void AxiDmaChannel::XAxiDmaSetSourceAddress(std::uint64_t buffer_address)
  {
    WriteReg(XAXIDMA_SRCADDR_OFFSET, static_cast<uint32_t>(buffer_address & 0xFFFFFFFF));
    WriteReg(XAXIDMA_SRCADDR_MSB_OFFSET, static_cast<uint32_t>((buffer_address >> 32) & 0xFFFFFFFF));
  }

  void AxiDmaChannel::XAxiDmaSetDestinationAddress(std::uint64_t buffer_address)
  {
    WriteReg(XAXIDMA_DESTADDR_OFFSET, static_cast<uint32_t>(buffer_address & 0xFFFFFFFF));
    WriteReg(XAXIDMA_DESTADDR_MSB_OFFSET, static_cast<uint32_t>((buffer_address >> 32) & 0xFFFFFFFF));
  }

  void AxiDmaChannel::XAxiDmaSelectCyclicMode(bool enable)
  {
    auto control_reg = ReadReg(XAXIDMA_CR_OFFSET);

    if (enable)
    {
      control_reg |= XAXIDMA_CR_CYCLIC_MASK;
    }
    else
    {
      control_reg &= ~XAXIDMA_CR_CYCLIC_MASK;
    }

    WriteReg(XAXIDMA_CR_OFFSET, control_reg);
  }

  void AxiDmaChannel::XAxiDmaSelectKeyHoleMode(bool enable)
  {
    uint32_t control_reg = ReadReg(XAXIDMA_CR_OFFSET);

    if (enable)
    {
      control_reg |= XAXIDMA_CR_KEYHOLE_MASK;
    }
    else
    {
      control_reg &= ~XAXIDMA_CR_KEYHOLE_MASK;
    }

    WriteReg(XAXIDMA_CR_OFFSET, control_reg);
  }

  void AxiDmaChannel::XAxiDmaIntrEnable(bool ioc, bool delay, bool error)
  {
    auto control_reg = ReadReg(XAXIDMA_CR_OFFSET);
    if (ioc)
    {
      control_reg |= XAXIDMA_IRQ_IOC_MASK;
    }
    if (delay)
    {
      control_reg |= XAXIDMA_IRQ_DELAY_MASK;
    }
    if (error)
    {
      control_reg |= XAXIDMA_IRQ_ERROR_MASK;
    }

    WriteReg(XAXIDMA_CR_OFFSET, control_reg);
  }

  void AxiDmaChannel::XAxiDmaIntrDisable(bool ioc, bool delay, bool error)
  {
    auto control_reg = ReadReg(XAXIDMA_CR_OFFSET);
    if (ioc)
    {
      control_reg &= ~XAXIDMA_IRQ_IOC_MASK;
    }
    if (delay)
    {
      control_reg &= ~XAXIDMA_IRQ_DELAY_MASK;
    }
    if (error)
    {
      control_reg &= ~XAXIDMA_IRQ_ERROR_MASK;
    }

    WriteReg(XAXIDMA_CR_OFFSET, control_reg);
  }

  void AxiDmaChannel::XAxiDmaAckInterrupts(std::uint32_t mask)
  {

    WriteReg(XAXIDMA_SR_OFFSET, mask & XAXIDMA_IRQ_ALL_MASK);
  }

  void AxiDmaChannel::XAxiDmaClearAllInterrupts()
  {
    WriteReg(XAXIDMA_SR_OFFSET, XAXIDMA_IRQ_ALL_MASK);
  }

  std::uint32_t AxiDmaChannel::XAxiDmaGetAllInterrupts()
  {
    return ReadReg(XAXIDMA_SR_OFFSET) & XAXIDMA_IRQ_ALL_MASK;
  }

  void AxiDmaChannel::XAxiDmaDisableAllInterrupts()
  {
    auto control_reg = ReadReg(XAXIDMA_CR_OFFSET);
    control_reg &= ~XAXIDMA_IRQ_ALL_MASK;
    WriteReg(XAXIDMA_CR_OFFSET, control_reg);
  }

  bool AxiDmaChannel::XAxiDmaIsStarted() const
  {
    return (ReadReg(XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK) == 0;
  }

  bool AxiDmaChannel::XAxiDmaIsIdle() const
  {
    return (ReadReg(XAXIDMA_SR_OFFSET) & XAXIDMA_IDLE_MASK) != 0;
  }

  bool AxiDmaChannel::XAxiDmaIsChannelAvailable() const
  {
    return (ReadReg(XAXIDMA_CR_OFFSET) & XAXIDMA_CR_RESERVED) != 0;
  }

  bool AxiDmaChannel::XAxiDmaIsScatterGatherIncluded() const
  {
    return (ReadReg(XAXIDMA_SR_OFFSET) & XAXIDMA_SG_INCLUDE_MASK) != 0;
  }

  std::uint32_t AxiDmaChannel::XAxiDmaStatusRead()
  {
    return ReadReg(XAXIDMA_SR_OFFSET);
  }

  void AxiDmaChannel::XAxiDmaLength(std::uint32_t length)
  {
    WriteReg(XAXIDMA_BUFFLEN_OFFSET, length);
  }

  void AxiDmaChannel::XAxiDmaPrintStatus()
  {
    auto status_reg = XAxiDmaStatusRead();
    SPDLOG_DEBUG("{} Status:", m_Direction == Direction::MM2S ? "MM2S" : "S2MM");
    SPDLOG_DEBUG("  halted={}", status_reg & 0x1);
    SPDLOG_DEBUG("  idle={}", (status_reg >> 1) & 0x1);
    SPDLOG_DEBUG("  sg_included={}", (status_reg >> 3) & 0x1);
    SPDLOG_DEBUG("  dma_internal_error={}", (status_reg >> 4) & 0x1);
    SPDLOG_DEBUG("  dma_slave_error={}", (status_reg >> 5) & 0x1);
    SPDLOG_DEBUG("  dma_decode_error={}", (status_reg >> 6) & 0x1);
    SPDLOG_DEBUG("  sg_internal_error={}", (status_reg >> 8) & 0x1);
    SPDLOG_DEBUG("  sg_slave_error={}", (status_reg >> 9) & 0x1);
    SPDLOG_DEBUG("  sg_decode_error={}", (status_reg >> 10) & 0x1);
    SPDLOG_DEBUG("  ioc_irq={}", (status_reg >> 12) & 0x1);
    SPDLOG_DEBUG("  dly_irq={}", (status_reg >> 13) & 0x1);
    SPDLOG_DEBUG("  err_irq={}", (status_reg >> 14) & 0x1);
    SPDLOG_DEBUG("  irq_threshold_sts={}", (status_reg >> 16) & 0xFF);
    SPDLOG_DEBUG("  irq_delay_sts={}", (status_reg >> 24) & 0xFF);
  }

  void AxiDmaChannel::XAxiDmaPrintControl()
  {
    auto control_reg = ReadReg(XAXIDMA_CR_OFFSET);
    SPDLOG_DEBUG("{} Control Register Bits:", m_Direction == Direction::MM2S ? "MM2S" : "S2MM");
    SPDLOG_DEBUG("  rs={}", control_reg & 0x1);
    SPDLOG_DEBUG("  reset={}", (control_reg >> 2) & 0x1);
    SPDLOG_DEBUG("  keyhole={}", (control_reg >> 3) & 0x1);
    SPDLOG_DEBUG("  cyclic_bd_enable={}", (control_reg >> 4) & 0x1);
    SPDLOG_DEBUG("  ioc_irq_enable={}", (control_reg >> 12) & 0x1);
    SPDLOG_DEBUG("  dly_irq_enable={}", (control_reg >> 13) & 0x1);
    SPDLOG_DEBUG("  err_irq_enable={}", (control_reg >> 14) & 0x1);
    SPDLOG_DEBUG("  irq_threshold={}", (control_reg >> 16) & 0xFF);
    SPDLOG_DEBUG("  irq_delay={}", (control_reg >> 24) & 0xFF);
  }

  std::uint32_t AxiDmaChannel::ReadReg(std::uint32_t offset) const
  {
    return m_Uio->Read(m_BaseAddress + offset);
  }
  std::uint64_t AxiDmaChannel::ReadReg64(std::uint32_t offset) const
  {
    return m_Uio->Read64(m_BaseAddress + offset);
  }
  void AxiDmaChannel::WriteReg(std::uint32_t offset, std::uint32_t value)
  {
    m_Uio->Write(m_BaseAddress + offset, value);
  }
  void AxiDmaChannel::WriteReg64(std::uint32_t offset, std::uint64_t value)
  {
    m_Uio->Write64(m_BaseAddress + offset, value);
  }
}