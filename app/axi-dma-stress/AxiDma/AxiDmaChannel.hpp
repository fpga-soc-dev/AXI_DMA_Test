#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include "Uio.hpp"

namespace Dma
{
    class AxiDmaChannel
    {
    public:
        friend class XAxiDmaBdRing;

        static constexpr uint32_t XAXIDMA_TX_OFFSET = 0x00000000;
        static constexpr uint32_t XAXIDMA_RX_OFFSET = 0x00000030;

        static constexpr uint32_t XAXIDMA_CR_OFFSET = 0x00000000;
        static constexpr uint32_t XAXIDMA_SR_OFFSET = 0x00000004;
        static constexpr uint32_t XAXIDMA_CDESC_OFFSET = 0x00000008;
        static constexpr uint32_t XAXIDMA_CDESC_MSB_OFFSET = 0x0000000C;
        static constexpr uint32_t XAXIDMA_TDESC_OFFSET = 0x00000010;
        static constexpr uint32_t XAXIDMA_TDESC_MSB_OFFSET = 0x00000014;
        static constexpr uint32_t XAXIDMA_SRCADDR_OFFSET = 0x00000018;
        static constexpr uint32_t XAXIDMA_SRCADDR_MSB_OFFSET = 0x0000001C;
        static constexpr uint32_t XAXIDMA_DESTADDR_OFFSET = 0x00000018;
        static constexpr uint32_t XAXIDMA_DESTADDR_MSB_OFFSET = 0x0000001C;
        static constexpr uint32_t XAXIDMA_BUFFLEN_OFFSET = 0x00000028;
        static constexpr uint32_t XAXIDMA_SGCTL_OFFSET = 0x0000002C;

        /* Bitmasks of XAXIDMA_CR_OFFSET register */
        static constexpr uint32_t XAXIDMA_CR_RUNSTOP_MASK = 0x00000001; /* Start/stop DMA channel */
        static constexpr uint32_t XAXIDMA_CR_RESERVED = 0x00000002;     /* Reserved - to check channel availability */
        static constexpr uint32_t XAXIDMA_CR_RESET_MASK = 0x00000004;   /* Reset DMA engine */
        static constexpr uint32_t XAXIDMA_CR_KEYHOLE_MASK = 0x00000008; /* Keyhole feature */
        static constexpr uint32_t XAXIDMA_CR_CYCLIC_MASK = 0x00000010;  /* Cyclic Mode */

        /* Bitmasks of XAXIDMA_SR_OFFSET register */
        static constexpr uint32_t XAXIDMA_HALTED_MASK = 0x00000001;       /* DMA channel halted */
        static constexpr uint32_t XAXIDMA_IDLE_MASK = 0x00000002;         /* DMA channel idle */
        static constexpr uint32_t XAXIDMA_ERR_INTERNAL_MASK = 0x00000010; /* Datamover internal err */
        static constexpr uint32_t XAXIDMA_ERR_SLAVE_MASK = 0x00000020;    /* Datamover slave err */
        static constexpr uint32_t XAXIDMA_ERR_DECODE_MASK = 0x00000040;   /* Datamover decode err */
        static constexpr uint32_t XAXIDMA_SG_INCLUDE_MASK = 0x00000008;   /* SG included */
        static constexpr uint32_t XAXIDMA_ERR_SG_INT_MASK = 0x00000100;   /* SG internal err */
        static constexpr uint32_t XAXIDMA_ERR_SG_SLV_MASK = 0x00000200;   /* SG slave err */
        static constexpr uint32_t XAXIDMA_ERR_SG_DEC_MASK = 0x00000400;   /* SG decode err */
        static constexpr uint32_t XAXIDMA_ERR_ALL_MASK = 0x00000770;      /* All errors */

        /* Multi-Channel DMA Descriptor Offsets */
        static constexpr uint32_t XAXIDMA_RX_CDESC0_OFFSET = 0x00000040;
        static constexpr uint32_t XAXIDMA_RX_CDESC0_MSB_OFFSET = 0x00000044;
        static constexpr uint32_t XAXIDMA_RX_TDESC0_OFFSET = 0x00000048;
        static constexpr uint32_t XAXIDMA_RX_TDESC0_MSB_OFFSET = 0x0000004C;
        static constexpr uint32_t XAXIDMA_RX_NDESC_OFFSET = 0x00000020;

        /* Bitmask for interrupts*/
        static constexpr uint32_t XAXIDMA_IRQ_IOC_MASK = 0x00001000;   /* Completion intr */
        static constexpr uint32_t XAXIDMA_IRQ_DELAY_MASK = 0x00002000; /* Delay interrupt */
        static constexpr uint32_t XAXIDMA_IRQ_ERROR_MASK = 0x00004000; /* Error interrupt */
        static constexpr uint32_t XAXIDMA_IRQ_ALL_MASK = 0x00007000;   /* All interrupts */

        enum class Direction
        {
            MM2S,
            S2MM
        };

        AxiDmaChannel(std::shared_ptr<UIO> uio, Direction direction);
        ~AxiDmaChannel() = default;

        AxiDmaChannel(const AxiDmaChannel &) = delete;
        AxiDmaChannel &operator=(const AxiDmaChannel &) = delete;
        AxiDmaChannel(AxiDmaChannel &&) = default;
        AxiDmaChannel &operator=(AxiDmaChannel &&) = default;

        bool XAxiDmaReset();
        void XAxiDmaStart();
        void XAxiDmaStop();
        void XAxiDmaIntrEnable(bool ioc, bool delay, bool error);
        void XAxiDmaIntrDisable(bool ioc, bool delay, bool error);
        void XAxiDmaAckInterrupts(std::uint32_t mask);
        void XAxiDmaClearAllInterrupts();
        std::uint32_t XAxiDmaGetAllInterrupts();
        void XAxiDmaDisableAllInterrupts();
        bool XAxiDmaIsStarted() const;
        bool XAxiDmaIsIdle() const;
        std::uint32_t XAxiDmaStatusRead();

        void XAxiDmaSetCurrentDescriptor(std::uint64_t desc_addr);
        std::uint32_t XAxiDmaGetCurrentDescriptorLow() const;
        std::uint32_t XAxiDmaGetCurrentDescriptorHigh() const;
        void XAxiDmaSetTailDescriptor(std::uint64_t desc_addr);
        std::uint32_t XAxiDmaGetTailDescriptorLow() const;
        std::uint32_t XAxiDmaGetTailDescriptorHigh() const;
        void XAxiDmaSetSourceAddress(std::uint64_t buffer_address);
        void XAxiDmaSetDestinationAddress(std::uint64_t buffer_address);
        void XAxiDmaSelectCyclicMode(bool enable);
        void XAxiDmaSelectKeyHoleMode(bool enable);
        bool XAxiDmaIsChannelAvailable() const;
        bool XAxiDmaIsScatterGatherIncluded() const;
        void XAxiDmaLength(std::uint32_t length);

        void XAxiDmaPrintStatus();
        void XAxiDmaPrintControl();

    private:
        std::shared_ptr<UIO> m_Uio;
        const Direction m_Direction;
        const std::uint32_t m_BaseAddress;

        std::uint32_t ReadReg(std::uint32_t offset) const;
        std::uint64_t ReadReg64(std::uint32_t offset) const;
        void WriteReg(std::uint32_t offset, std::uint32_t value);
        void WriteReg64(std::uint32_t offset, std::uint64_t value);
    };
}