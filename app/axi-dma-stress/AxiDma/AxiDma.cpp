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

    bool AxiDma::InitializeSimpleModeConfig(ICacheManager *cache_manager, std::string rx_buffer_name, std::string tx_buffer_name)
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
            // ConfigureTxDataBuffer(tx_buffer_name);
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
            ConfigureRxDataBuffer(rx_buffer_name);
        }

        m_IsInitialized = true;
        return true;
    }

    bool AxiDma::InitializeCyclicModeConfig(ICacheManager *cache_manager,
                                            std::string rx_data_buffer_name,
                                            std::string rx_ring_buffer_name,
                                            std::string tx_data_buffer_name,
                                            std::string tx_ring_buffer_name)
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

        if (m_CacheManager)
        {
            SPDLOG_WARN("Cache manager is already set");
        }
        else
        {
            m_CacheManager = cache_manager;
        }

        if (m_Config.Mm2SNumChannels > 1 || m_Config.S2MmNumChannels > 1)
        {
            max_transfer_len = XAXIDMA_MCHAN_MAX_TRANSFER_LEN;
        }
        else
        {
            max_transfer_len = (1U << m_Config.BufferLengthRegister) - 1;
        }

        m_TxBdRing = std::make_shared<XAxiDmaBdRing>(m_TxChannel, cache_manager, tx_ring_buffer_name);
        m_RxBdRing = std::make_shared<XAxiDmaBdRing>(m_RxChannel, cache_manager, rx_ring_buffer_name);

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
            // ConfigureTxDataBuffer(tx_data_buffer_name);
            // ConfigureTxRingBuffer(tx_ring_buffer_name);
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
            ConfigureRxDataBuffer(rx_data_buffer_name);
            ConfigureRxRingBuffer(rx_ring_buffer_name);
        }

        m_IsInitialized = true;
        return true;
    }

    void AxiDma::ConfigureRxDataBuffer(std::string buffer_name)
    {
        if (!m_CacheManager)
        {
            SPDLOG_ERROR("Cache manager is not set not set");
            return;
        }
        auto buffer_size = m_CacheManager->GetBufferSize(buffer_name);
        if (buffer_size == 0)
        {
            SPDLOG_ERROR("RX buffer {} not found", buffer_name);
            return;
        }

        m_RxDataBufferConfig.buffer_name = buffer_name;
        m_RxDataBufferConfig.size = buffer_size;
        SPDLOG_INFO("Configured RX buffer: {} (size: {})", buffer_name, buffer_size);
    }

    void AxiDma::ConfigureRxRingBuffer(std::string buffer_name)
    {
        if (!m_CacheManager)
        {
            SPDLOG_ERROR("Cache manager not set");
            return;
        }
        auto buffer_size = m_CacheManager->GetBufferSize(buffer_name);
        if (buffer_size == 0)
        {
            SPDLOG_ERROR("RX ring buffer {} not found", buffer_name);
            return;
        }

        m_RxRingBufferConfig.buffer_name = buffer_name;
        m_RxRingBufferConfig.size = buffer_size;
        SPDLOG_INFO("Configured RX ring buffer: {} (size: {})", buffer_name, buffer_size);
    }

    void AxiDma::AttachInterruptRxHandler(const std::function<void()> &callback, bool queue_enabled)
    {
        if (callback == nullptr)
        {
            SPDLOG_ERROR("Callback function is null");
            return;
        }

        m_RxCallback = callback;
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

        if ((irq_status & AxiDmaChannel::XAXIDMA_IRQ_ERROR_MASK) ||
            !(irq_status & AxiDmaChannel::XAXIDMA_IRQ_ALL_MASK))
        {
            if (m_Config.HasScatterGather)
            {
                m_RxBdRing->XAxiDmaBdRingDumpAllBds();
            }

            SPDLOG_ERROR("DMA S2MM Error Interrupt occurred.");
            m_RxChannel->XAxiDmaPrintStatus();
            m_RxChannel->XAxiDmaReset();

            return;
        }

        if (m_Config.HasScatterGather && m_RxBdRing && m_CacheManager)
        {

            std::byte *buffer_base_virt = m_CacheManager->BufferPtr(m_RxDataBufferConfig.buffer_name);
            int bd_count = m_RxBdRing->m_AllCnt;

            std::uint32_t current_desc = m_RxChannel->XAxiDmaGetCurrentDescriptorLow();
            std::uint32_t first_bd_phys = static_cast<std::uint32_t>(m_RxBdRing->m_FirstBdPhysAddr);

            int current_bd_index = (current_desc - first_bd_phys) / m_RxBdRing->m_Separation;
            int completed_bd_index = (current_bd_index - 1 + bd_count) % bd_count;

            std::size_t data_offset = completed_bd_index * m_RxFrameSize;
            std::size_t bd_offset = completed_bd_index * m_RxBdRing->m_Separation;

            m_CurrentRxBufferAddr = reinterpret_cast<std::uintptr_t>(buffer_base_virt + data_offset);

            m_CacheManager->InvalidateCache(m_RxRingBufferConfig.buffer_name);

            m_CacheManager->InvalidateCache(m_RxDataBufferConfig.buffer_name,
                                            data_offset, m_RxFrameSize,
                                            UDmaBuffer::SyncDirection::DMA_FROM_DEVICE);
        }
        else if (!m_Config.HasScatterGather && m_CacheManager)
        {
            m_CacheManager->InvalidateCache(m_RxDataBufferConfig.buffer_name);
        }

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

        m_CacheManager->FlushCache(m_RxDataBufferConfig.buffer_name);
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
        m_CurrentRxBufferAddr = rx_buffer_address;

        std::uint32_t frame_size = m_CacheManager->GetBufferSize(m_RxDataBufferConfig.buffer_name) / bd_count;
        m_RxFrameSize = frame_size;

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

            status = bd_curr_ptr.XAxiDmaBdSetLength(frame_size);
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
            SPDLOG_DEBUG("RX Data Buffer[{}] configured, buf_addr={:08x}, virt_addr={:08x}", index,
                         (unsigned int)(rx_buffer_address - frame_size),
                         (std::uintptr_t)(m_CacheManager->BufferPtr(m_RxDataBufferConfig.buffer_name) + (index * frame_size)));
        }

        status = m_RxBdRing->XAxiDmaBdRingSetCoalesce(1, 100);
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

    std::uint64_t AxiDma::GetCurrentDescriptor() const
    {
        auto low = m_RxChannel->XAxiDmaGetCurrentDescriptorLow();
        auto high = m_RxChannel->XAxiDmaGetCurrentDescriptorHigh();
        return (static_cast<std::uint64_t>(high) << 32) | low;
    }

    std::uint64_t AxiDma::GetTailDescriptor() const
    {
        auto low = m_RxChannel->XAxiDmaGetTailDescriptorLow();
        auto high = m_RxChannel->XAxiDmaGetTailDescriptorHigh();
        return (static_cast<std::uint64_t>(high) << 32) | low;
    }

    std::uint32_t *AxiDma::GetCurrentBufferAddress() const
    {
        return reinterpret_cast<std::uint32_t *>(m_CurrentRxBufferAddr);
    }

} // namespace Dma