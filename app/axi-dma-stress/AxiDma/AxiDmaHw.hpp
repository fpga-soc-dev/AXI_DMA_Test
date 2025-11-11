#pragma once

#include <cstdint>

namespace Dma
{

    static constexpr uint32_t XAXIDMA_MCHAN_MAX_TRANSFER_LEN = 0x00FFFF; /* Max length MCDMA hw supports */

    static constexpr uint32_t XAXIDMA_DESC_LSB_MASK = (0xFFFFFFC0U); /* LSB Address mask */

    static constexpr uint32_t XAXIDMA_DELAY_MASK = 0xFF000000;    /* Delay timeout counter */
    static constexpr uint32_t XAXIDMA_COALESCE_MASK = 0x00FF0000; /* Coalesce counter */
    static constexpr uint32_t XAXIDMA_DELAY_SHIFT = 24;
    static constexpr uint32_t XAXIDMA_COALESCE_SHIFT = 16;

} // namespace Dma