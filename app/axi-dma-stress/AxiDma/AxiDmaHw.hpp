#pragma once

#include <cstdint>

namespace Dma
{

    static constexpr uint32_t XAXIDMA_MICROMODE_MIN_BUF_ALIGN = 0xFFF; /* Minimum byte alignment requirement for buffer address in Micro DMA mode */

    static constexpr uint32_t XAXIDMA_BD_CTRL_TXSOF_MASK = 0x08000000; /**< First tx packet */
    static constexpr uint32_t XAXIDMA_BD_CTRL_TXEOF_MASK = 0x04000000; /**< Last tx packet */
    static constexpr uint32_t XAXIDMA_BD_CTRL_ALL_MASK = 0x0C000000;   /**< All control bits */

    static constexpr uint32_t XAXIDMA_TX_OFFSET = 0x00000000;
    static constexpr uint32_t XAXIDMA_RX_OFFSET = 0x00000030;

    static constexpr uint32_t XAXIDMA_CR_OFFSET = 0x00000000;
    static constexpr uint32_t XAXIDMA_SR_OFFSET = 0x00000004;
    static constexpr uint32_t XAXIDMA_CDESC_OFFSET = 0x00000008;
    static constexpr uint32_t XAXIDMA_CDESC_MSB_OFFSET = 0x0000000C;
    static constexpr uint32_t XAXIDMA_TDESC_OFFSET = 0x00000010;
    static constexpr uint32_t XAXIDMA_TDESC_MSB_OFFSET = 0x00000014;
    static constexpr uint32_t XAXIDMA_SRCADDR_OFFSET = 0x00000018;
    static constexpr uint32_t XAXIDMA_SRCADDR_MSB_OFFSET = 0x0000001C;
    static constexpr uint32_t XAXIDMA_DESTADDR_OFFSET = 0x00000018;
    static constexpr uint32_t XAXIDMA_DESTADDR_MSB_OFFSET = 0x0000001C;
    static constexpr uint32_t XAXIDMA_BUFFLEN_OFFSET = 0x00000028;
    static constexpr uint32_t XAXIDMA_SGCTL_OFFSET = 0x0000002C;

    /* Multi-Channel DMA Descriptor Offsets */
    static constexpr uint32_t XAXIDMA_RX_CDESC0_OFFSET = 0x00000040;
    static constexpr uint32_t XAXIDMA_RX_CDESC0_MSB_OFFSET = 0x00000044;
    static constexpr uint32_t XAXIDMA_RX_TDESC0_OFFSET = 0x00000048;
    static constexpr uint32_t XAXIDMA_RX_TDESC0_MSB_OFFSET = 0x0000004C;
    static constexpr uint32_t XAXIDMA_RX_NDESC_OFFSET = 0x00000020;

    /* Bitmasks of XAXIDMA_CR_OFFSET register */
    static constexpr uint32_t XAXIDMA_CR_RUNSTOP_MASK = 0x00000001; /* Start/stop DMA channel */
    static constexpr uint32_t XAXIDMA_CR_RESERVED = 0x00000002;     /* Reserved - to check channel availability */
    static constexpr uint32_t XAXIDMA_CR_RESET_MASK = 0x00000004;   /* Reset DMA engine */
    static constexpr uint32_t XAXIDMA_CR_KEYHOLE_MASK = 0x00000008; /* Keyhole feature */
    static constexpr uint32_t XAXIDMA_CR_CYCLIC_MASK = 0x00000010;  /* Cyclic Mode */

    /* Bitmasks of XAXIDMA_SR_OFFSET register */
    static constexpr uint32_t XAXIDMA_HALTED_MASK = 0x00000001;       /* DMA channel halted */
    static constexpr uint32_t XAXIDMA_IDLE_MASK = 0x00000002;         /* DMA channel idle */
    static constexpr uint32_t XAXIDMA_ERR_INTERNAL_MASK = 0x00000010; /* Datamover internal err */
    static constexpr uint32_t XAXIDMA_ERR_SLAVE_MASK = 0x00000020;    /* Datamover slave err */
    static constexpr uint32_t XAXIDMA_ERR_DECODE_MASK = 0x00000040;   /* Datamover decode err */
    static constexpr uint32_t XAXIDMA_SG_INCLUDE_MASK = 0x00000008;   /* SG included */
    static constexpr uint32_t XAXIDMA_ERR_SG_INT_MASK = 0x00000100;   /* SG internal err */
    static constexpr uint32_t XAXIDMA_ERR_SG_SLV_MASK = 0x00000200;   /* SG slave err */
    static constexpr uint32_t XAXIDMA_ERR_SG_DEC_MASK = 0x00000400;   /* SG decode err */
    static constexpr uint32_t XAXIDMA_ERR_ALL_MASK = 0x00000770;      /* All errors */

    /* Bitmask for interrupts*/
    static constexpr uint32_t XAXIDMA_IRQ_IOC_MASK = 0x00001000;   /* Completion intr */
    static constexpr uint32_t XAXIDMA_IRQ_DELAY_MASK = 0x00002000; /* Delay interrupt */
    static constexpr uint32_t XAXIDMA_IRQ_ERROR_MASK = 0x00004000; /* Error interrupt */
    static constexpr uint32_t XAXIDMA_IRQ_ALL_MASK = 0x00007000;   /* All interrupts */

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

    static constexpr uint32_t XAXIDMA_BD_USR0_OFFSET = 0x20; /* User IP specific word0 */
    static constexpr uint32_t XAXIDMA_BD_USR1_OFFSET = 0x24; /* User IP specific word1 */
    static constexpr uint32_t XAXIDMA_BD_USR2_OFFSET = 0x28; /* User IP specific word2 */
    static constexpr uint32_t XAXIDMA_BD_USR3_OFFSET = 0x2C; /* User IP specific word3 */
    static constexpr uint32_t XAXIDMA_BD_USR4_OFFSET = 0x30; /* User IP specific word4 */

    static constexpr uint32_t XAXIDMA_BD_ID_OFFSET = 0x34;           /* Sw ID */
    static constexpr uint32_t XAXIDMA_BD_HAS_STSCNTRL_OFFSET = 0x38; /* Whether has stscntrl strm */
    static constexpr uint32_t XAXIDMA_BD_HAS_DRE_OFFSET = 0x3C;      /* Whether has DRE */

    static constexpr uint32_t XAXIDMA_BD_HAS_DRE_MASK = 0xF00; /* Whether has DRE mask */
    static constexpr uint32_t XAXIDMA_BD_WORDLEN_MASK = 0xFF;  /* Whether has DRE mask */

    static constexpr uint32_t XAXIDMA_BD_HAS_DRE_SHIFT = 8; /* Whether has DRE shift */
    static constexpr uint32_t XAXIDMA_BD_WORDLEN_SHIFT = 0; /* Whether has DRE shift */

    static constexpr uint32_t XAXIDMA_BD_START_CLEAR = 8;     /* Offset to start clear */
    static constexpr uint32_t XAXIDMA_BD_BYTES_TO_CLEAR = 48; /* BD specific bytes to be cleared */

    static constexpr uint32_t XAXIDMA_BD_NUM_WORDS = 16U;   /* Total number of words for one BD */
    static constexpr uint32_t XAXIDMA_BD_HW_NUM_BYTES = 52; /* Number of bytes hw used */

    static constexpr uint32_t XAXIDMA_LAST_APPWORD = 4; /* The offset of the last app word. */

    /*Bitmasks of XAXIDMA_BD_STS_OFFSET register */
    static constexpr uint32_t XAXIDMA_BD_STS_COMPLETE_MASK = 0x80000000; /* Completed */
    static constexpr uint32_t XAXIDMA_BD_STS_DEC_ERR_MASK = 0x40000000;  /* Decode error */
    static constexpr uint32_t XAXIDMA_BD_STS_SLV_ERR_MASK = 0x20000000;  /* Slave error */
    static constexpr uint32_t XAXIDMA_BD_STS_INT_ERR_MASK = 0x10000000;  /* Internal err */
    static constexpr uint32_t XAXIDMA_BD_STS_ALL_ERR_MASK = 0x70000000;  /* All errors */
    static constexpr uint32_t XAXIDMA_BD_STS_RXSOF_MASK = 0x08000000;    /* First rx pkt */
    static constexpr uint32_t XAXIDMA_BD_STS_RXEOF_MASK = 0x04000000;    /* Last rx pkt */
    static constexpr uint32_t XAXIDMA_BD_STS_ALL_MASK = 0xFC000000;      /* All status bits */

    static constexpr uint32_t XAXIDMA_MCHAN_MAX_TRANSFER_LEN = 0x00FFFF; /* Max length MCDMA hw supports */

    static constexpr uint32_t XAXIDMA_BD_MINIMUM_ALIGNMENT = 0x40; /* Minimum byte alignment requirement for descriptors to satisfy both hardware/software needs */

    static constexpr uint32_t XAXIDMA_DESC_LSB_MASK = (0xFFFFFFC0U); /* LSB Address mask */

    static constexpr uint32_t XAXIDMA_DELAY_MASK = 0xFF000000;    /* Delay timeout counter */
    static constexpr uint32_t XAXIDMA_COALESCE_MASK = 0x00FF0000; /* Coalesce counter */
    static constexpr uint32_t XAXIDMA_DELAY_SHIFT = 24;
    static constexpr uint32_t XAXIDMA_COALESCE_SHIFT = 16;

} // namespace Dma