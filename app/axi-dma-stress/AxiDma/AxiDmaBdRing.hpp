#pragma once

#include <cstdint>
#include <memory>
#include "AxiDmaBd.hpp"
#include "AxiDmaHw.hpp"
#include "Uio.hpp"
#include "AxiDmaChannel.hpp"
#include "UDmaBuffer.hpp"

namespace Dma
{

    class XAxiDmaBdRing
    {
    public:
        static constexpr uint32_t AXIDMA_CHANNEL_NOT_HALTED = 1;
        static constexpr uint32_t AXIDMA_CHANNEL_HALTED = 2;

        /* Argument constant to simplify argument setting*/
        static constexpr uint32_t XAXIDMA_NO_CHANGE = 0xFFFFFFFF;
        static constexpr uint32_t XAXIDMA_ALL_BDS = 0x0FFFFFFF; /* 268 Million */
    public:
        friend class AxiDma;

        explicit XAxiDmaBdRing(std::shared_ptr<AxiDmaChannel> channel, ICacheManager *cache_manager, const std::string &bd_buffer_name);
        ~XAxiDmaBdRing() = default;

        void SetCacheManager(ICacheManager *cache_manager);
        void XAxiDmaBdRingSnapShotCurrBd();

        std::uint32_t XAxiDmaBdRingCntCalc(std::uint32_t alignment, std::uint32_t buffer_size);

        bool XAxiDmaBdRingCreate(std::uintptr_t phys_addr, std::uintptr_t virt_addr, std::uint32_t alignment, int bd_count);
        bool XAxiDmaBdRingClone(std::uint8_t *src_bd_ptr);
        bool XAxiDmaBdRingAlloc(int num_bd, AxiDmaBd &bd_ptr);
        bool XAxiDmaBdRingSetCoalesce(std::uint32_t counter, std::uint32_t timer);
        bool XAxiDmaBdRingToHw(int num_bd, std::uint8_t *bd_set_ptr);
        bool XAxiDmaBdRingStart();
        bool XAxiDmaUpdateBdRingCDesc();
        bool XAxiDmaBdRingHwIsStarted();
        bool XAxiDmaStartBdRingHw();
        void XAxiDmaBdRingDumpRegs();
        void XAxiDmaBdRingDumpAllBds();
        int XAxiDmaBdRingFromHw(int bd_limit, std::uint8_t **bd_set_ptr);

    private:
        std::shared_ptr<AxiDmaChannel> m_Channel;
        ICacheManager *m_CacheManager{nullptr};
        std::string m_BdBufferName;

        bool m_IsRxChannel{false};     /* Is this a receive channel */
        volatile int m_RunState{0};    /* Whether channel is running */
        bool m_HasStsCntrlStrm{false}; /* Whether has stscntrl stream */
        bool m_HasDRE{false};          /* Whether has Data Realignment Engine */
        int m_DataWidth{0};
        bool m_Addr_ext{false}; /* Whether address is extended beyond 32 bits */
        std::uint32_t m_MaxTransferLen{0};

        std::uintptr_t m_FirstBdPhysAddr{0}; /* Physical address of 1st BD in list */
        std::uintptr_t m_FirstBdAddr{0};     /* Virtual address of 1st BD in list */
        std::uintptr_t m_LastBdAddr{0};      /* Virtual address of last BD in the list */
        std::uint32_t m_Length{0};           /* Total size of ring in bytes */
        std::uintptr_t m_Separation{0};      /* Number of bytes between the starting address of adjacent BDs */
        std::uintptr_t m_LastBdPhysAddr{0};  /* Physical address of last BD in list */
        AxiDmaBd m_FreeHead;                 /* First BD in the free group */
        AxiDmaBd m_PreHead;                  /* First BD in the pre-work group */
        AxiDmaBd m_HwHead;                   /* First BD in the work group */
        AxiDmaBd m_HwTail;                   /* Last BD in the work group */
        AxiDmaBd m_PostHead;                 /* First BD in the post-work group */
        AxiDmaBd m_BdRestart;                /* BD to load when channel is started */

        int m_FreeCnt{0};   /* Number of allocatable BDs in free group */
        int m_PreCnt{0};    /* Number of BDs in pre-work group */
        int m_HwCnt{0};     /* Number of BDs in work group */
        int m_PostCnt{0};   /* Number of BDs in post-work group */
        int m_AllCnt{0};    /* Total Number of BDs for channel */
        int m_RingIndex{0}; /* Ring Index */
        int m_Cyclic{0};    /* Check for cyclic DMA Mode */

        void FlushBdCache(std::uint8_t *bd_ptr, int num_bd);
        void InvalidateBdCache(std::uint8_t *bd_ptr, int num_bd);

        void XAxiDmaRingSeekAhead(AxiDmaBd &bd_ptr, int num_bd);
        std::uint8_t *XAxiDmaBdRingNext(const std::uint8_t *bd);
        std::uintptr_t XAxiDmaVirtToPhys(const std::uint8_t *bd_ptr);
        std::uintptr_t XAxiDmaVirtToPhys(std::uintptr_t bd_addr);
    };

} // namespace Dma