#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <sys/ioctl.h>

namespace Dma
{

    enum class DmaProxyDirection
    {
        RX,
        TX
    };

    struct DmaProxyChannelConfig
    {
        DmaProxyDirection direction{};
        std::string device_name;
        std::size_t buffer_count{};     // e.g. RX_BUFFER_COUNT
        std::size_t buffer_increment{}; // e.g. BUFFER_INCREMENT
        std::size_t buffer_size{};      // e.g. BUFFER_SIZE
    };

    static constexpr std::size_t DMA_PROXY_BUFFER_SIZE = (128 * 1024); /* must match driver exactly */
    static constexpr std::size_t DMA_PROXY_BUFFER_COUNT = 32;          /* driver only */
    static constexpr std::size_t DMA_PROXY_TX_BUFFER_COUNT = 1;        /* app only, must be <= to the number in the driver */
    static constexpr std::size_t DMA_PROXY_RX_BUFFER_COUNT = 32;       /* app only, must be <= to the number in the driver */
    static constexpr std::size_t DMA_PROXY_BUFFER_INCREMENT = 1;       /* normally 1, but skipping buffers (2) defeats prefetching in the CPU */

    static constexpr auto DMA_PROXY_IOCTL_FINISH_XFER = _IOW('a', 'a', int32_t *);
    static constexpr auto DMA_PROXY_IOCTL_START_XFER = _IOW('a', 'b', int32_t *);
    static constexpr auto DMA_PROXY_IOCTL_XFER = _IOR('a', 'c', int32_t *);

    enum class ProxyStatus : uint32_t
    {
        NO_ERROR = 0,
        BUSY = 1,
        TIMEOUT = 2,
        ERROR = 3
    };

    struct alignas(1024) ChannelBuffer
    {
        // Must match driver: buffer consists of 32-bit words
        std::uint32_t buffer[DMA_PROXY_BUFFER_SIZE / sizeof(std::uint32_t)];
        ProxyStatus status;
        std::uint32_t length;
    };

    static_assert(sizeof(ChannelBuffer) % 1024 == 0, "ChannelBuffer must be 1024-byte aligned and multiple of 1024 bytes.");

    class AxiDmaProxy
    {
    public:
        using BufferView = std::span<std::byte>;
        using RxCallback = std::function<void(BufferView, std::uint64_t transfer_index)>;

        explicit AxiDmaProxy(const DmaProxyChannelConfig &rx_config, const DmaProxyChannelConfig &tx_config);
        ~AxiDmaProxy();

        AxiDmaProxy(const AxiDmaProxy &) = delete;
        AxiDmaProxy &operator=(const AxiDmaProxy &) = delete;

        void RxChannelCyclicStart(std::uint32_t bytes_per_transfer, RxCallback cb);
        void RxStop();

    private:
        struct Channel
        {
            ChannelBuffer *buffer_ptr;
            int fd;
        };
        DmaProxyChannelConfig m_RxConfig;
        // DmaProxyChannelConfig m_TxConfig;
        Channel m_RxChannel{nullptr, -1};
        // Channel m_TxChannel{nullptr, -1};
        std::atomic<bool> m_Running{false};
    };
}