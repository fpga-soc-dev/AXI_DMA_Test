#pragma once

#include <array>
#include "AxiDmaHw.hpp"

namespace Dma
{
    // using XAxiDma_Bd = std::uint32_t[XAXIDMA_BD_NUM_WORDS];

    class AxiDmaBd
    {
    public:
        /** Buffer Descriptor offsets
         *  USR* fields are defined by higher level IP.
         *  setup for EMAC type devices. The first 13 words are used by hardware.
         *  All words after the 13rd word are for software use only.
         *  @{
         */
        static constexpr uint32_t XAXIDMA_BD_NDESC_OFFSET = 0x00;        /* Next descriptor pointer */
        static constexpr uint32_t XAXIDMA_BD_NDESC_MSB_OFFSET = 0x04;    /* Next descriptor pointer */
        static constexpr uint32_t XAXIDMA_BD_BUFA_OFFSET = 0x08;         /* Buffer address */
        static constexpr uint32_t XAXIDMA_BD_BUFA_MSB_OFFSET = 0x0C;     /* Buffer address */
        static constexpr uint32_t XAXIDMA_BD_MCCTL_OFFSET = 0x10;        /* Multichannel Control Fields */
        static constexpr uint32_t XAXIDMA_BD_STRIDE_VSIZE_OFFSET = 0x14; /* 2D Transfer Sizes */
        static constexpr uint32_t XAXIDMA_BD_CTRL_LEN_OFFSET = 0x18;     /* Control/buffer length */
        static constexpr uint32_t XAXIDMA_BD_STS_OFFSET = 0x1C;          /* Status */
        static constexpr uint32_t XAXIDMA_BD_USR0_OFFSET = 0x20;         /* User IP specific word0 */
        static constexpr uint32_t XAXIDMA_BD_USR1_OFFSET = 0x24;         /* User IP specific word1 */
        static constexpr uint32_t XAXIDMA_BD_USR2_OFFSET = 0x28;         /* User IP specific word2 */
        static constexpr uint32_t XAXIDMA_BD_USR3_OFFSET = 0x2C;         /* User IP specific word3 */
        static constexpr uint32_t XAXIDMA_BD_USR4_OFFSET = 0x30;         /* User IP specific word4 */

        static constexpr uint32_t XAXIDMA_BD_HAS_DRE_OFFSET = 0x3C;        /* Whether has DRE */
        static constexpr uint32_t XAXIDMA_BD_WORDLEN_MASK = 0xFF;          /* Whether has DRE mask */
        static constexpr uint32_t XAXIDMA_BD_HAS_DRE_MASK = 0xF00;         /* Whether has DRE mask */
        static constexpr uint32_t XAXIDMA_MICROMODE_MIN_BUF_ALIGN = 0xFFF; /* Minimum byte alignment requirement for buffer address in Micro DMA mode */
        static constexpr uint32_t XAXIDMA_BD_HAS_STSCNTRL_OFFSET = 0x38;   /* Whether has stscntrl strm */

        static constexpr uint32_t XAXIDMA_LAST_APPWORD = 4;   /* The offset of the last app word. */
        static constexpr uint32_t XAXIDMA_BD_NUM_WORDS = 16U; /* Total number of words for one BD */

        static constexpr uint32_t XAXIDMA_BD_CTRL_TXSOF_MASK = 0x08000000; /**< First tx packet */
        static constexpr uint32_t XAXIDMA_BD_CTRL_TXEOF_MASK = 0x04000000; /**< Last tx packet */
        static constexpr uint32_t XAXIDMA_BD_CTRL_ALL_MASK = 0x0C000000;   /**< All control bits */

        /*Bitmasks of XAXIDMA_BD_STS_OFFSET register */
        static constexpr uint32_t XAXIDMA_BD_STS_COMPLETE_MASK = 0x80000000; /* Completed */
        static constexpr uint32_t XAXIDMA_BD_STS_DEC_ERR_MASK = 0x40000000;  /* Decode error */
        static constexpr uint32_t XAXIDMA_BD_STS_SLV_ERR_MASK = 0x20000000;  /* Slave error */
        static constexpr uint32_t XAXIDMA_BD_STS_INT_ERR_MASK = 0x10000000;  /* Internal err */
        static constexpr uint32_t XAXIDMA_BD_STS_ALL_ERR_MASK = 0x70000000;  /* All errors */
        static constexpr uint32_t XAXIDMA_BD_STS_RXSOF_MASK = 0x08000000;    /* First rx pkt */
        static constexpr uint32_t XAXIDMA_BD_STS_RXEOF_MASK = 0x04000000;    /* Last rx pkt */
        static constexpr uint32_t XAXIDMA_BD_STS_ALL_MASK = 0xFC000000;      /* All status bits */

        static constexpr uint32_t XAXIDMA_BD_ID_OFFSET = 0x34;         /* Sw ID */
        static constexpr uint32_t XAXIDMA_BD_HAS_DRE_SHIFT = 8;        /* Whether has DRE shift */
        static constexpr uint32_t XAXIDMA_BD_WORDLEN_SHIFT = 0;        /* Whether has DRE shift */
        static constexpr uint32_t XAXIDMA_BD_START_CLEAR = 8;          /* Offset to start clear */
        static constexpr uint32_t XAXIDMA_BD_BYTES_TO_CLEAR = 48;      /* BD specific bytes to be cleared */
        static constexpr uint32_t XAXIDMA_BD_HW_NUM_BYTES = 52;        /* Number of bytes hw used */
        static constexpr uint32_t XAXIDMA_BD_MINIMUM_ALIGNMENT = 0x40; /* Minimum byte alignment requirement for descriptors to satisfy both hardware/software needs */

    public:
        AxiDmaBd();

        void SetBaseAddress(std::uint8_t *base_addr);
        std::uint8_t *GetBaseAddress() const;
        bool XAxiDmaBdSetLength(std::uint32_t len_bytes, std::uint32_t len_mask);
        bool XAxiDmaBdSetBufAddr(std::uint64_t phys_addr);
        bool XAxiDmaBdSetBufAddrMicroMode(std::uint64_t phys_addr);

        bool XAxiDmaBdSetAppWord(std::uint32_t index, std::uint32_t value);
        std::uint32_t XAxiDmaBdGetAppWord(std::uint32_t index, int &valid) const;
        void XAxiDmaBdSetCtrl(std::uint32_t ctrl);

        void XAxiDmaBdDump();
        void XAxiDmaBdClear();
        std::uint32_t XAxiDmaBdGetCtrl();
        std::uint32_t XAxiDmaBdGetSts();
        std::uint32_t XAxiDmaBdGetLength(std::uint32_t length_mask);
        void XAxiDmaBdSetId(std::uint32_t id);
        bool XAxiDmaBdHwCompleted();

    private:
        std::uint8_t *m_Base;

        std::uint32_t XAxiDmaBdRead(std::uint32_t offset) const;
        void XAxiDmaBdWrite(std::uint32_t offset, std::uint32_t data);
        void XAxiDmaBdWrite64(std::uint32_t offset, std::uint64_t data);
    };
} // namespace Dma