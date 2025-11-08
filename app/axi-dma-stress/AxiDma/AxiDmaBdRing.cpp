#include "AxiDmaBdRing.hpp"

namespace Dma
{

    XAxiDmaBdRing::XAxiDmaBdRing(std::shared_ptr<UIO> uio_device) : m_UioDevice(uio_device)
    {
    }

    void XAxiDmaBdRing::XAxiDmaBdRingSnapShotCurrBd()
    {
        // TX Channel
        if (!m_IsRxChannel)
        {
            if (!m_Addr_ext)
            {
                m_BdRestart = reinterpret_cast<XAxiDma_Bd *>(static_cast<std::uintptr_t>(m_UioDevice->Read(m_ChanBaseOffset + XAXIDMA_CDESC_OFFSET)));
            }
            else
            {
                auto lower_addr = m_UioDevice->Read(m_ChanBaseOffset + XAXIDMA_CDESC_OFFSET);
                auto upper_addr = m_UioDevice->Read(m_ChanBaseOffset + XAXIDMA_CDESC_MSB_OFFSET);
                auto bd_addr = reinterpret_cast<XAxiDma_Bd *>(static_cast<std::uintptr_t>(((static_cast<std::uintptr_t>(upper_addr) << 32) | lower_addr)));
                m_BdRestart = bd_addr;
            }
            return;
        }

        // RX Channel
        if (!m_RingIndex)
        {
            // first RX ring
            if (!m_Addr_ext)
            {
                m_BdRestart = reinterpret_cast<XAxiDma_Bd *>(static_cast<std::uintptr_t>(m_UioDevice->Read(m_ChanBaseOffset + XAXIDMA_CDESC_OFFSET)));
            }
            else
            {
                auto lower_addr = m_UioDevice->Read(m_ChanBaseOffset + XAXIDMA_CDESC_OFFSET);
                auto upper_addr = m_UioDevice->Read(m_ChanBaseOffset + XAXIDMA_CDESC_MSB_OFFSET);
                auto bd_addr = reinterpret_cast<XAxiDma_Bd *>(static_cast<std::uintptr_t>(((static_cast<std::uintptr_t>(upper_addr) << 32) | lower_addr)));
                m_BdRestart = bd_addr;
            }
        }
        else
        {
            if (!m_Addr_ext)
            {
                auto off = XAXIDMA_RX_CDESC0_OFFSET + (m_RingIndex - 1) * XAXIDMA_RX_NDESC_OFFSET;
                m_BdRestart = reinterpret_cast<XAxiDma_Bd *>(static_cast<std::uintptr_t>(m_UioDevice->Read(m_ChanBaseOffset + off)));
            }
            else
            {
                auto lower_addr = m_UioDevice->Read(m_ChanBaseOffset + XAXIDMA_CDESC_OFFSET);
                auto upper_addr = m_UioDevice->Read(m_ChanBaseOffset + XAXIDMA_RX_CDESC0_MSB_OFFSET + (m_RingIndex - 1) * XAXIDMA_RX_NDESC_OFFSET);
                auto bd_addr = static_cast<std::uintptr_t>(lower_addr | (static_cast<std::uintptr_t>(upper_addr) << 32));
                m_BdRestart = reinterpret_cast<XAxiDma_Bd *>(bd_addr);
            }
        }
    }
} // namespace Dma