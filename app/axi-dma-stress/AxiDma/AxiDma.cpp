#include "AxiDma.hpp"
#include "AxiDmaHw.hpp"
#include <spdlog/spdlog.h>

namespace Dma
{
    AxiDma::AxiDma(const std::string &uio_device,
                   Config config,
                   UDmaBufferAllocate::UdmaBufConfig udma_buf_config,
                   UDmaBuffer::CachePolicy cache_policy)

        : m_UioDevice(std::make_shared<UIO>(uio_device)),
          m_TxChannel(std::make_shared<AxiDmaChannel>(m_UioDevice, AxiDmaChannel::Direction::MM2S)),
          m_RxChannel(std::make_shared<AxiDmaChannel>(m_UioDevice, AxiDmaChannel::Direction::S2MM)),
          m_Config(config),
          m_UdmaBuffer(cache_policy),
          m_TxBdRing(m_TxChannel, m_UdmaBuffer),
          m_RxBdRing(m_RxChannel, m_UdmaBuffer)
    {
        if (!m_UdmaBuffer.CreateBuffer(udma_buf_config))
        {
            SPDLOG_ERROR("Failed to create UDmaBuffer for AxiDma.");
        }

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
        m_Config.HasMm2s = m_TxChannel->XAxiDmaIsChannelAvailable();
        m_Config.HasS2mm = m_RxChannel->XAxiDmaIsChannelAvailable();
        m_Config.HasScatterGather = m_TxChannel->XAxiDmaIsScatterGatherIncluded() || m_RxChannel->XAxiDmaIsScatterGatherIncluded();

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

        if (m_Config.HasMm2s)
        {
            m_TxChannel->XAxiDmaReset();
            m_TxChannel->XAxiDmaIntrDisable(true, true, true);
        }

        if (m_Config.HasS2mm)
        {
            m_RxChannel->XAxiDmaReset();
            m_RxChannel->XAxiDmaIntrDisable(true, true, true);
        }

        m_IsInitialized = true;
        return true;
    }

    void AttachInterruptRxHandler(const std::function<void()> &callback, bool queue_enabled)
    {
        m_RxChannel->AttachInterruptHandler(callback, queue_enabled);
    }

    void DetachRxInterruptHandler()
    {
        m_RxChannel->DetachInterruptHandler();
    }

    void AttachInterruptTxHandler(const std::function<void()> &callback, bool queue_enabled = false)
    {
        m_TxChannel->AttachInterruptHandler(callback, queue_enabled);
    }

    void DetachTxInterruptHandler()
    {
        m_TxChannel->DetachInterruptHandler();
    }

#ifdef UNUSED
    bool AxiDma::XAxiDmaSimpleTransfer(std::uint64_t buffer_address, std::uint32_t length)
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
#endif

    void AxiDma::Stop()
    {
        if (m_Config.HasMm2s)
        {
            m_TxChannel->XAxiDmaStop();
            m_TxChannel->XAxiDmaClearAllInterrupts();
            m_TxChannel->XAxiDmaCDisableAllInterrupts();
        }

        if (m_Config.HasS2mm)
        {
            m_RxChannel->XAxiDmaStop();
            m_RxChannel->XAxiDmaClearAllInterrupts();
            m_RxChannel->XAxiDmaCDisableAllInterrupts();
        }
    }

#ifdef UNUSED
    bool AxiDma::XAxiDmaCyclicTransfer(UDmaBuffer &buffers)
    {
        auto bd_ring_buf_length = buffers.GetBufferSize("udmabuf0");
        auto bdring_buf_phyaddr = buffers.GetPhysicalAddress("udmabuf0");
        auto bdring_buf_virtaddr = reinterpret_cast<std::uintptr_t>(buffers.BufferPtr("udmabuf0"));

        auto data_buffer_length = buffers.GetBufferSize("udmabuf1");
        auto data_buffer_phyaddr = buffers.GetPhysicalAddress("udmabuf1");
        auto data_buffer_virtaddr = reinterpret_cast<std::uintptr_t>(buffers.BufferPtr("udmabuf1"));

        if (!m_Config.HasScatterGather)
        {
            SPDLOG_ERROR("Cyclic transfer is not supported in Simple mode.");
            return false;
        }

        if (direction == Direction::MM2S)
        {
        }
        else if (direction == Direction::S2MM)
        {
            bool status;
            XAxiDma_Bd bd_template{};
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
            // AxiDmaBd::XAxiDmaBdClear(reinterpret_cast<std::uintptr_t>(bd_template));
            /*
            status = m_RxBdRing.XAxiDmaBdRingClone(&bd_template);
            if (!status)
            {
                SPDLOG_ERROR("Rx bd clone failed.");
                return false;
            }
            */

            free_bd_count = m_RxBdRing.m_FreeCnt;
            status = m_RxBdRing.XAxiDmaBdRingAlloc(free_bd_count, &bd_ptr);
            if (!status)
            {
                SPDLOG_ERROR("Rx bd allocation failed.");
                return false;
            }

            bd_curr_ptr = bd_ptr;
            rx_buffer_address = data_buffer_phyaddr;

            std::uint32_t frame_size = data_buffer_length / bd_count;

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

                status = AxiDmaBd::XAxiDmaBdSetLength(reinterpret_cast<std::uintptr_t>(bd_curr_ptr), frame_size, m_RxBdRing.m_MaxTransferLen);
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

                rx_buffer_address += frame_size;
                bd_curr_ptr = m_RxBdRing.XAxiDmaBdRingNext(reinterpret_cast<const XAxiDma_Bd *>(bd_curr_ptr));
                SPDLOG_DEBUG("RX BD[{}] configured, buf_addr={:08x}, length={}", index,
                             (unsigned int)(rx_buffer_address - frame_size),
                             frame_size);
            }

            SPDLOG_DEBUG("Flushing RX BD ring cache.");
            buffers.FlushCache("udmabuf0");

            status = m_RxBdRing.XAxiDmaBdRingSetCoalesce(16, 1);
            if (!status)
            {
                SPDLOG_ERROR("Failed to set coalesce for Rx BD ring.");
                return false;
            }

            buffers.InvalidateCache("udmabuf0");
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
#endif

} // namespace Dma