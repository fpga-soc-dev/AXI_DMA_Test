#pragma once

#include <cstdint>
#include <memory>
#include "AxiDmaBd.hpp"
#include "AxiDmaHw.hpp"
#include "Uio.hpp"

namespace Dma
{

    class XAxiDmaBdRing
    {
    public:
        friend class AxiDma;

        static constexpr uint32_t AXIDMA_CHANNEL_NOT_HALTED = 1;
        static constexpr uint32_t AXIDMA_CHANNEL_HALTED = 2;

        /* Argument constant to simplify argument setting*/
        static constexpr uint32_t XAXIDMA_NO_CHANGE = 0xFFFFFFFF;
        static constexpr uint32_t XAXIDMA_ALL_BDS = 0x0FFFFFFF; /* 268 Million */
    public:
        explicit XAxiDmaBdRing(std::shared_ptr<UIO> uio_device);
        ~XAxiDmaBdRing();

        void XAxiDmaBdRingSnapShotCurrBd();

        std::uint32_t XAxiDmaBdRingCntCalc(std::uint32_t alignment, std::uint32_t buffer_size);

        bool XAxiDmaBdRingCreate(std::uintptr_t phys_addr, std::uintptr_t virt_addr, std::uint32_t alignment, int bd_count);
        bool XAxiDmaBdRingClone(XAxiDma_Bd *src_bd_ptr);
        bool XAxiDmaBdRingAlloc(int num_bd, XAxiDma_Bd **bd_ptr);
        bool XAxiDmaBdRingSetCoalesce(std::uint32_t counter, std::uint32_t timer);
        bool XAxiDmaBdRingToHw(int num_bd, XAxiDma_Bd *bd_set_ptr);
        bool XAxiDmaBdRingStart();
        bool XAxiDmaUpdateBdRingCDesc();
        bool XAxiDmaBdRingHwIsStarted();
        bool XAxiDmaStartBdRingHw();

    private:
        std::shared_ptr<UIO> m_UioDevice;
        std::uint32_t m_ChanBaseOffset{0}; /* chain base offset*/
        bool m_IsRxChannel{false};         /* Is this a receive channel */
        volatile int m_RunState{0};        /* Whether channel is running */
        bool m_HasStsCntrlStrm{false};     /* Whether has stscntrl stream */
        bool m_HasDRE{false};              /* Whether has Data Realignment Engine */
        int m_DataWidth{0};
        bool m_Addr_ext{false}; /* Whether address is extended beyond 32 bits */
        std::uint32_t m_MaxTransferLen{0};

        std::uintptr_t m_FirstBdPhysAddr{0}; /* Physical address of 1st BD in list */
        std::uintptr_t m_FirstBdAddr{0};     /* Virtual address of 1st BD in list */
        std::uintptr_t m_LastBdAddr{0};      /* Virtual address of last BD in the list */
        std::uint32_t m_Length{0};           /* Total size of ring in bytes */
        std::uintptr_t m_Separation{0};      /* Number of bytes between the starting address of adjacent BDs */
        std::uintptr_t m_LastBdPhysAddr{0};  /* Physical address of last BD in list */
        XAxiDma_Bd *m_FreeHead;              /* First BD in the free group */
        XAxiDma_Bd *m_PreHead;               /* First BD in the pre-work group */
        XAxiDma_Bd *m_HwHead;                /* First BD in the work group */
        XAxiDma_Bd *m_HwTail;                /* Last BD in the work group */
        XAxiDma_Bd *m_PostHead;              /* First BD in the post-work group */
        XAxiDma_Bd *m_BdRestart;             /* BD to load when channel is started */
        // XAxiDma_Bd *m_CyclicBd;              /* Useful for Cyclic DMA operations */
        int m_FreeCnt{0};   /* Number of allocatable BDs in free group */
        int m_PreCnt{0};    /* Number of BDs in pre-work group */
        int m_HwCnt{0};     /* Number of BDs in work group */
        int m_PostCnt{0};   /* Number of BDs in post-work group */
        int m_AllCnt{0};    /* Total Number of BDs for channel */
        int m_RingIndex{0}; /* Ring Index */
        int m_Cyclic{0};    /* Check for cyclic DMA Mode */

        void XAxiDmaRingSeekAhead(XAxiDma_Bd **bd_ptr, int num_bd);
        XAxiDma_Bd *XAxiDmaBdRingNext(const XAxiDma_Bd *bd);
        std::uintptr_t XAxiDmaVirtToPhys(const XAxiDma_Bd *bd_ptr);
        std::uintptr_t XAxiDmaVirtToPhys(std::uintptr_t bd_addr);
    };

} // namespace Dma