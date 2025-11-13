#include "AxiDma.hpp"
#include "AxiDmaHw.hpp"
#include <spdlog/spdlog.h>

namespace Dma
{
    AxiDma::AxiDma(const std::string &uio_device, Config config)
        : m_UioDevice(std::make_shared<UIO>(uio_device)),
          m_TxChannel(std::make_shared<AxiDmaChannel>(m_UioDevice, AxiDmaChannel::Direction::MM2S)),
          m_RxChannel(std::make_shared<AxiDmaChannel>(m_UioDevice, AxiDmaChannel::Direction::S2MM)),
          m_Config(config)
    {
    }

    AxiDma::~AxiDma()
    {
        Stop();
    }

    bool AxiDma::InitializeSimpleModeConfig(ICacheManager *cache_manager, const BufferConfig &rx_config, const BufferConfig &tx_config)
    {
        uint32_t max_transfer_len{0};
        m_IsInitialized = false;
        m_Config.HasMm2s = m_TxChannel->XAxiDmaIsChannelAvailable();
        m_Config.HasS2mm = m_RxChannel->XAxiDmaIsChannelAvailable();
        m_Config.HasScatterGather = m_TxChannel->XAxiDmaIsScatterGatherIncluded() || m_RxChannel->XAxiDmaIsScatterGatherIncluded();

        if (m_CacheManager)
        {
            SPDLOG_WARN("Cache manager is already set");
        }
        else
        {
            m_CacheManager = cache_manager;
        }

        if (m_Config.HasMm2s)
        {
            if (!m_TxChannel->XAxiDmaReset())
            {
                SPDLOG_ERROR("Failed to reset MM2S channel.");
                return false;
            }
            m_TxChannel->XAxiDmaIntrDisable(true, true, true);
            m_TxChannel->XAxiDmaIntrEnable(true, true, true);
            // ConfigureTxDataBuffer(tx_config);
        }

        if (m_Config.HasS2mm)
        {
            if (!m_RxChannel->XAxiDmaReset())
            {
                SPDLOG_ERROR("Failed to reset S2MM channel.");
                return false;
            }
            m_RxChannel->XAxiDmaIntrDisable(true, true, true);
            m_RxChannel->XAxiDmaIntrEnable(true, true, true);
            ConfigureRxDataBuffer(rx_config);
        }

        m_IsInitialized = true;
        return true;
    }

    bool AxiDma::InitializeCyclicModeConfig(ICacheManager *cache_manager,
                                            const BufferConfig &rx_data_config,
                                            const BufferConfig &rx_ring_config,
                                            const BufferConfig &tx_data_config,
                                            const BufferConfig &tx_ring_config)
    {
        uint32_t max_transfer_len{0};
        m_IsInitialized = false;
        m_Config.HasMm2s = m_TxChannel->XAxiDmaIsChannelAvailable();
        m_Config.HasS2mm = m_RxChannel->XAxiDmaIsChannelAvailable();
        m_Config.HasScatterGather = m_TxChannel->XAxiDmaIsScatterGatherIncluded() || m_RxChannel->XAxiDmaIsScatterGatherIncluded();
        if (!m_Config.HasScatterGather)
        {
            SPDLOG_ERROR("Cyclic mode requires Scatter-Gather capability, but DMA device does not support it.");
            return false;
        }

        if (m_Config.Mm2SNumChannels > 1 || m_Config.S2MmNumChannels > 1)
        {
            max_transfer_len = XAXIDMA_MCHAN_MAX_TRANSFER_LEN;
        }
        else
        {
            max_transfer_len = (1U << m_Config.BufferLengthRegister) - 1;
        }

        m_TxBdRing = std::make_shared<XAxiDmaBdRing>(m_TxChannel, cache_manager, tx_ring_config.buffer_name);
        m_RxBdRing = std::make_shared<XAxiDmaBdRing>(m_RxChannel, cache_manager, rx_ring_config.buffer_name);

        m_TxBdRing->m_RunState = XAxiDmaBdRing::AXIDMA_CHANNEL_HALTED;
        m_TxBdRing->m_IsRxChannel = false;
        if (!m_Config.MicroDmaChannel)
        {
            m_TxBdRing->m_MaxTransferLen = max_transfer_len;
        }
        else
        {
            /* In MicroDMA mode, Maximum length that can be transferred
             * is '(Memory Data Width / 8) * Burst Size' */
            m_TxBdRing->m_MaxTransferLen = ((m_Config.Mm2SDataWidth / 8) * m_Config.Mm2SBurstSize);
        }

        m_TxBdRing->m_RingIndex = 0;

        m_RxBdRing->m_RunState = XAxiDmaBdRing::AXIDMA_CHANNEL_HALTED;
        m_RxBdRing->m_IsRxChannel = true;
        m_RxBdRing->m_RingIndex = 0;

        if (m_Config.HasMm2s)
        {
            SPDLOG_INFO("Configuring MM2S channel.");
            m_TxBdRing->m_HasStsCntrlStrm = m_Config.HasStsCntrlStrm;
            if (m_Config.AddressWidth > 32)
            {
                m_TxBdRing->m_Addr_ext = true;
            }
            else
            {
                m_TxBdRing->m_Addr_ext = false;
            }

            m_TxBdRing->m_HasDRE = m_Config.HasMm2sDRE;
            m_TxBdRing->m_DataWidth = (m_Config.Mm2SDataWidth >> 3);
        }

        if (m_Config.HasS2mm)
        {
            SPDLOG_INFO("Configuring S2MM channel.");
            m_RxBdRing->m_HasStsCntrlStrm = m_Config.HasStsCntrlStrm;
            m_RxBdRing->m_HasDRE = m_Config.HasS2mmDRE;
            m_RxBdRing->m_DataWidth = (m_Config.S2MmDataWidth >> 3);

            if (!m_Config.MicroDmaChannel)
            {
                m_RxBdRing->m_MaxTransferLen = max_transfer_len;
            }
            else
            {
                /* In MicroDMA mode, Maximum length that can be transferred is '(Memory Data Width / 8) * Burst Size' */
                m_RxBdRing->m_MaxTransferLen = ((m_Config.S2MmDataWidth / 8) * m_Config.S2MmBurstSize);
            }

            if (m_Config.AddressWidth > 32)
            {
                m_RxBdRing->m_Addr_ext = true;
            }
            else
            {
                m_RxBdRing->m_Addr_ext = false;
            }
        }

        if (m_Config.HasMm2s)
        {
            if (!m_TxChannel->XAxiDmaReset())
            {
                SPDLOG_ERROR("Failed to reset MM2S channel.");
                return false;
            }
            m_TxChannel->XAxiDmaIntrDisable(true, true, true);
            m_TxChannel->XAxiDmaIntrEnable(true, true, true);
            // ConfigureTxDataBuffer(tx_data_config);
            // ConfigureTxRingBuffer(tx_ring_config);
        }

        if (m_Config.HasS2mm)
        {
            if (!m_RxChannel->XAxiDmaReset())
            {
                SPDLOG_ERROR("Failed to reset S2MM channel.");
                return false;
            }
            m_RxChannel->XAxiDmaIntrDisable(true, true, true);
            m_RxChannel->XAxiDmaIntrEnable(true, true, true);
            ConfigureRxDataBuffer(rx_data_config);
            ConfigureRxRingBuffer(rx_ring_config);
        }

        m_IsInitialized = true;
        return true;
    }

    void AxiDma::ConfigureRxDataBuffer(const BufferConfig &config)
    {
        if (!m_CacheManager)
        {
            SPDLOG_ERROR("Cache manager is not set not set");
            return;
        }
        auto buffer_size = m_CacheManager->GetBufferSize(config.buffer_name);
        if (buffer_size == 0)
        {
            SPDLOG_ERROR("RX buffer {} not found", config.buffer_name);
            return;
        }

        m_RxDataBufferConfig = config;
        SPDLOG_INFO("Configured RX buffer: {} (size: {})", config.buffer_name, buffer_size);
    }

    void AxiDma::ConfigureRxRingBuffer(const BufferConfig &config)
    {
        if (!m_CacheManager)
        {
            SPDLOG_ERROR("Cache manager not set");
            return;
        }
        auto buffer_size = m_CacheManager->GetBufferSize(config.buffer_name);
        if (buffer_size == 0)
        {
            SPDLOG_ERROR("RX ring buffer {} not found", config.buffer_name);
            return;
        }

        m_RxRingBufferConfig = config;
        SPDLOG_INFO("Configured RX ring buffer: {} (size: {})", config.buffer_name, buffer_size);
    }

    void AxiDma::PrepareTxDataBuffer()
    {
        if (m_CacheManager && !m_TxDataBufferConfig.buffer_name.empty())
        {
            m_CacheManager->FlushCache(m_TxDataBufferConfig.buffer_name);
        }
    }

    void AxiDma::PrepareRxDataBuffer()
    {
        if (m_CacheManager && !m_RxDataBufferConfig.buffer_name.empty())
        {
            m_CacheManager->InvalidateCache(m_RxDataBufferConfig.buffer_name);
        }
    }

    void AxiDma::AttachInterruptRxHandler(const std::function<void()> &callback, bool queue_enabled)
    {
        if (callback == nullptr)
        {
            SPDLOG_ERROR("Callback function is null");
            return;
        }

        m_RxCallback = callback;
        // m_Uio->InterruptAttach([this]()
        //                              { OnMm2sInterrupt(); },
        //                              queue_enabled);

        m_UioDevice->InterruptAttach([this]()
                                     { OnRxInterrupt(); },
                                     queue_enabled);
    }

    void AxiDma::DetachRxInterruptHandler()
    {
        m_UioDevice->InterruptDetach();
        m_RxCallback = nullptr;
    }

    void AxiDma::OnRxInterrupt()
    {

        auto irq_status = m_RxChannel->XAxiDmaGetAllInterrupts();
        m_RxChannel->XAxiDmaAckInterrupts(irq_status);

        if (!(irq_status & AxiDmaChannel::XAXIDMA_IRQ_ALL_MASK))
        {
            return;
        }

        if ((irq_status & AxiDmaChannel::XAXIDMA_IRQ_ERROR_MASK))
        {
            // if (m_Config.HasScatterGather)
            //{
            //   m_RxBdRing.XAxiDmaBdRingDumpRegs();
            // }
            //  else
            //{
            //  XAxiDmaPrintStatus(Direction::S2MM, XAxiDmaDumpStatus(Direction::S2MM, status_reg));
            // }
            SPDLOG_ERROR("DMA S2MM Error Interrupt occurred.");
            m_RxChannel->XAxiDmaPrintStatus();
            m_RxChannel->XAxiDmaReset();

            return;
        }

        /*
        if (m_Config.HasScatterGather)
        {
            if ((irq_status & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK)))
            {
                if (m_Callback)
                {
                    m_Callback();
                }
            }
        }
*/
        if (m_RxCallback)
        {
            m_RxCallback();
        }
    }

    bool AxiDma::XAxiDmaRxSimpleTransfer(std::uint32_t length)
    {
        std::uint32_t word;
        if (m_IsInitialized == false)
        {
            SPDLOG_ERROR("DMA not initialized");
            return false;
        }

        if (m_Config.HasScatterGather)
        {
            SPDLOG_ERROR("Simple transfer is not supported in Scatter-Gather mode.");
            return false;
        }

        if (!m_CacheManager || m_RxDataBufferConfig.buffer_name.empty())
        {
            SPDLOG_ERROR("Cache manager or RX buffer not configured");
            return false;
        }

        auto buffer_size = m_CacheManager->GetBufferSize(m_RxDataBufferConfig.buffer_name);
        if (length > buffer_size)
        {
            SPDLOG_ERROR("Transfer length ({}) exceeds buffer size ({})", length, buffer_size);
            return false;
        }

        PrepareRxDataBuffer();
        auto phys_addr = m_CacheManager->GetPhysicalAddress(m_RxDataBufferConfig.buffer_name);

        /*
        if (length > m_RxBdRing.m_MaxTransferLen)
        {
            SPDLOG_ERROR("Transfer length {} exceeds maximum allowed length {} for S2MM.", length, m_RxBdRing.m_MaxTransferLen);
            return false;
        }
*/
        if (!m_Config.HasS2mm)
        {
            SPDLOG_ERROR("Simple transfer is not supported for S2MM.");
            return false;
        }
        /* If the engine is doing transfer, cannot submit */
        if (m_RxChannel->XAxiDmaIsStarted())
        {
            if (!m_RxChannel->XAxiDmaIsIdle())
            {
                SPDLOG_ERROR("DMA S2MM engine is currently running. Cannot submit new transfer.");
                return false;
            }
        }

        // if (!m_Config.MicroDmaChannel)
        //{
        //     word = static_cast<uint32_t>((m_RxBdRing.m_DataWidth) - 1);
        // }
        // else
        //{
        //     word = XAXIDMA_MICROMODE_MIN_BUF_ALIGN;
        // }

        auto buffer_address = m_CacheManager->GetPhysicalAddress("udmabuf0");
        if (buffer_address == 0)
        {
            SPDLOG_ERROR("Failed to get physical address for RX buffer {}", m_RxDataBufferConfig.buffer_name);
            return false;
        }
        /*
        if ((buffer_address & word) != 0)
        {
            if (!m_Config.HasS2mmDRE)
            {
                SPDLOG_ERROR("Unaligned transfer without DRE %x\r\n", buffer_address);
                return false;
            }
        }
        */
        m_RxChannel->XAxiDmaSetDestinationAddress(buffer_address);
        m_RxChannel->XAxiDmaStart();
        m_RxChannel->XAxiDmaLength(length);

        return true;
    }

    void AxiDma::Stop()
    {
        if (m_Config.HasMm2s)
        {
            m_TxChannel->XAxiDmaStop();
            m_TxChannel->XAxiDmaClearAllInterrupts();
            m_TxChannel->XAxiDmaDisableAllInterrupts();
        }

        if (m_Config.HasS2mm)
        {
            m_RxChannel->XAxiDmaStop();
            m_RxChannel->XAxiDmaClearAllInterrupts();
            m_RxChannel->XAxiDmaDisableAllInterrupts();
        }
    }

    bool AxiDma::RxBdRingSetup()
    {
        if (!m_Config.HasScatterGather)
        {
            SPDLOG_ERROR("Cyclic transfer is not supported in Simple mode.");
            return false;
        }

        /* Disable all RX interrupts before RxBD space setup */
        m_RxChannel->XAxiDmaIntrDisable(true, true, true);
        /* Setup Rx BD space */
        auto bd_count = m_RxBdRing->XAxiDmaBdRingCntCalc(AxiDmaBd::XAXIDMA_BD_MINIMUM_ALIGNMENT, m_RxRingBufferConfig.size);
        SPDLOG_DEBUG("Cyclic RX BD count calculated: {}", bd_count);

        auto rx_bd_ring_phyaddr = m_CacheManager->GetPhysicalAddress(m_RxRingBufferConfig.buffer_name);
        auto rx_bd_ring_virtaddr = reinterpret_cast<std::uintptr_t>(m_CacheManager->BufferPtr(m_RxRingBufferConfig.buffer_name));

        auto status = m_RxBdRing->XAxiDmaBdRingCreate(rx_bd_ring_phyaddr, rx_bd_ring_virtaddr, AxiDmaBd::XAXIDMA_BD_MINIMUM_ALIGNMENT, bd_count);
        if (!status)
        {
            SPDLOG_ERROR("Failed to create BD Ring for cyclic transfer.");
            return false;
        }

        AxiDmaBd bd_template;
        std::uint8_t template_memory[AxiDmaBd::BD_SIZE_BYTES] = {0};
        bd_template.SetBaseAddress(template_memory);
        /*Setup a BD template for the Rx channel. Then copy it to every RX BD.*/

        bd_template.XAxiDmaBdClear();
        status = m_RxBdRing->XAxiDmaBdRingClone(bd_template.GetBaseAddress());
        if (!status)
        {
            SPDLOG_ERROR("Rx bd clone failed.");
            return false;
        }

        AxiDmaBd bd_ptr;

        auto free_bd_count = m_RxBdRing->m_FreeCnt;
        status = m_RxBdRing->XAxiDmaBdRingAlloc(free_bd_count, bd_ptr);
        if (!status)
        {
            SPDLOG_ERROR("Rx bd allocation failed.");
            return false;
        }

        AxiDmaBd bd_curr_ptr;
        bd_curr_ptr.SetBaseAddress(bd_ptr.GetBaseAddress());

        auto rx_buffer_address = m_CacheManager->GetPhysicalAddress(m_RxDataBufferConfig.buffer_name);

        std::uint32_t frame_size = m_CacheManager->GetBufferSize(m_RxDataBufferConfig.buffer_name) / bd_count;

        for (int index = 0; index < free_bd_count; ++index)
        {
            status = bd_curr_ptr.XAxiDmaBdSetBufAddr(rx_buffer_address);
            if (!status)
            {
                SPDLOG_ERROR("Rx set buffer address failed for buf_addr={:08x} bd_ptr={:08x} status={}",
                             (unsigned int)rx_buffer_address,
                             (std::uintptr_t)bd_curr_ptr.GetBaseAddress(), status);
                return false;
            }

            status = bd_curr_ptr.XAxiDmaBdSetLength(frame_size, m_RxBdRing->m_MaxTransferLen);
            if (!status)
            {
                SPDLOG_ERROR("Rx set length failed for length={} bd_ptr={:08x} status={}",
                             frame_size,
                             (std::uintptr_t)bd_curr_ptr.GetBaseAddress(), status);
                return false;
            }

            /* Receive BDs do not need to set anything for the control
             * The hardware will set the SOF/EOF bits per stream status
             */

            bd_curr_ptr.XAxiDmaBdSetCtrl(0);
            bd_curr_ptr.XAxiDmaBdSetId(rx_buffer_address);

            rx_buffer_address += frame_size;
            bd_curr_ptr.SetBaseAddress(m_RxBdRing->XAxiDmaBdRingNext(bd_curr_ptr.GetBaseAddress()));
            SPDLOG_DEBUG("RX BD[{}] configured, buf_addr={:08x}, length={}", index,
                         (unsigned int)(rx_buffer_address - frame_size),
                         frame_size);
        }

        status = m_RxBdRing->XAxiDmaBdRingSetCoalesce(16, 1);
        if (!status)
        {
            SPDLOG_ERROR("Failed to set coalesce for Rx BD ring.");
            return false;
        }

        status = m_RxBdRing->XAxiDmaBdRingToHw(free_bd_count, bd_ptr.GetBaseAddress());
        if (!status)
        {
            SPDLOG_ERROR("Failed to submit Rx BD ring to hardware.");
            return false;
        }

        m_RxChannel->XAxiDmaIntrEnable(true, true, true);
        /* Enable Cyclic DMA mode */
        m_RxBdRing->m_Cyclic = true;
        m_RxChannel->XAxiDmaSelectCyclicMode(true);

        /* Start RX DMA channel */
        status = m_RxBdRing->XAxiDmaBdRingStart();
        if (!status)
        {
            SPDLOG_ERROR("Failed to start Rx BD ring.");
            return false;
        }

        return true;
    }

    bool AxiDma::XAxiDmaRxCyclicTransfer()
    {
        RxBdRingSetup();
        return true;
    }
    /*
        std::uint32_t AxiDma::XAxiDmaGetBdCount()
        {
            if (m_Config.HasScatterGather)
            {
                XAxiDma_Bd *bd_btr;
                if (direction == Direction::MM2S)
                {
                    return m_TxBdRing.XAxiDmaBdRingFromHw(XAXIDMA_ALL_BDS, &bd_btr);
                }
                else if (direction == Direction::S2MM)
                {
                    return m_RxBdRing.XAxiDmaBdRingFromHw(XAXIDMA_ALL_BDS, &bd_btr);
                }
            }
            return 0;
        }
            */

} // namespace Dma