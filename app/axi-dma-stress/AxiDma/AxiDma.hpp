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
        bool InitializeSimpleModeConfig(ICacheManager *cache_manager, const BufferConfig &rx_config, const BufferConfig &tx_config);

        bool XAxiDmaRxCyclicTransfer();
        bool InitializeCyclicModeConfig(ICacheManager *cache_manager,
                                        const BufferConfig &rx_data_config,
                                        const BufferConfig &rx_ring_config,
                                        const BufferConfig &tx_data_config,
                                        const BufferConfig &tx_ring_config);

        void AttachInterruptRxHandler(const std::function<void()> &callback, bool queue_enabled = false);
        void DetachRxInterruptHandler();

        // void AttachInterruptTxHandler(const std::function<void()> &callback, bool queue_enabled = false);
        // void DetachTxInterruptHandler();

        // std::uint32_t XAxiDmaGetBdCount();

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

        void PrepareTxDataBuffer(); // FlushCache
        void PrepareRxDataBuffer(); // InvalidateCache

        void OnRxInterrupt();

        void ConfigureRxDataBuffer(const BufferConfig &config);
        void ConfigureTxDataBuffer(const BufferConfig &config);
        void ConfigureRxRingBuffer(const BufferConfig &config);
        void ConfigureTxRingBuffer(const BufferConfig &config);

        bool RxBdRingSetup();
        bool TxBdRingSetup();
    };
}