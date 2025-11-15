#pragma once

#include <memory>
#include <string>
#include "AxiDmaBdRing.hpp"
#include "AxiDmaChannel.hpp"
#include "UDmaBuffer.hpp"

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

        struct BufferConfig
        {
            std::string buffer_name;
            std::size_t size{0};
        };

    public:
        AxiDma(const std::string &uio_device, Config config);
        ~AxiDma();

        void Stop();

        bool XAxiDmaRxSimpleTransfer(std::uint32_t length);
        bool InitializeSimpleModeConfig(ICacheManager *cache_manager, std::string rx_buffer_name, std::string tx_buffer_name);
        void AttachInterruptRxHandler(const std::function<void()> &callback, bool queue_enabled = false);
        std::uint64_t GetCurrentDescriptor() const;
        std::uint64_t GetTailDescriptor() const;
        std::uint32_t *GetCurrentBufferAddress() const;

        bool XAxiDmaRxCyclicTransfer();
        bool InitializeCyclicModeConfig(ICacheManager *cache_manager,
                                        std::string rx_data_buffer_name,
                                        std::string rx_ring_buffer_name,
                                        std::string tx_data_buffer_name,
                                        std::string tx_ring_buffer_name);

        void DetachRxInterruptHandler();

        // void AttachInterruptTxHandler(const std::function<void()> &callback, bool queue_enabled = false);
        // void DetachTxInterruptHandler();

    private:
        std::shared_ptr<UIO> m_UioDevice;
        std::shared_ptr<AxiDmaChannel> m_TxChannel;
        std::shared_ptr<XAxiDmaBdRing> m_TxBdRing;
        BufferConfig m_TxDataBufferConfig;
        BufferConfig m_TxRingBufferConfig;

        std::shared_ptr<AxiDmaChannel> m_RxChannel;
        std::shared_ptr<XAxiDmaBdRing> m_RxBdRing;
        BufferConfig m_RxDataBufferConfig;
        BufferConfig m_RxRingBufferConfig;

        ICacheManager *m_CacheManager{nullptr};

        std::function<void()> m_RxCallback;
        std::function<void()> m_TxCallback;

        Config m_Config;
        bool m_IsInitialized{false};
        std::uintptr_t m_CurrentRxBufferAddr{0};
        std::uint32_t m_RxFrameSize{0};
        std::uint64_t m_RxDataBaseAddr{0};
        std::uint64_t m_RxRingBaseAddr{0};

        void OnRxInterrupt();

        void ConfigureRxDataBuffer(std::string config);
        void ConfigureRxRingBuffer(std::string config);
        void ConfigureTxDataBuffer(std::string config);
        void ConfigureTxRingBuffer(std::string config);

        bool RxBdRingSetup();
        bool TxBdRingSetup();
    };
}