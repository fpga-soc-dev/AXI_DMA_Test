#pragma once

#include <memory>
#include <string>
#include "AxiDmaBdRing.hpp"
#include "UDmaBuffer.hpp"
#include "AxiDmaChannel.hpp"

namespace Dma
{
    class AxiDma
    {
    public:
        struct Config
        {
            bool HasStsCntrlStrm{false};
            bool HasMm2s{false};
            bool HasMm2sDRE{false};
            int Mm2SDataWidth{0};
            bool HasS2mm{false};
            bool HasS2mmDRE{false};
            int S2MmDataWidth{32};
            bool HasScatterGather{false};
            int Mm2SNumChannels{0};
            int S2MmNumChannels{1};
            int Mm2SBurstSize{0};
            int S2MmBurstSize{256};
            bool MicroDmaChannel{false};
            int AddressWidth{32};
            int BufferLengthRegister{26};
        };

    public:
        AxiDma(const std::string &uio_device, Config config, UDmaBufferAllocate::UdmaBufConfig udma_buf_config, UDmaBuffer::CachePolicy cache_policy);
        ~AxiDma();

        void Stop();

        bool XAxiDmaSimpleTransfer(std::uint64_t buffer_address, std::uint32_t length);
        bool XAxiDmaCyclicTransfer(UDmaBuffer &buffers);

        void AttachInterruptRxHandler(const std::function<void()> &callback, bool queue_enabled = false);
        void DetachRxInterruptHandler();

        void AttachInterruptTxHandler(const std::function<void()> &callback, bool queue_enabled = false);
        void DetachTxInterruptHandler();

        std::uint32_t XAxiDmaGetBdCount();

    private:
        std::shared_ptr<UIO> m_UioDevice;
        std::shared_ptr<AxiDmaChannel> m_TxChannel;
        std::shared_ptr<AxiDmaChannel> m_RxChannel;
        UDmaBuffer m_UdmaBuffer;

        Config m_Config;
        bool m_IsInitialized{false};
        XAxiDmaBdRing m_TxBdRing;
        XAxiDmaBdRing m_RxBdRing;

        bool InitializeConfig();
    };
}