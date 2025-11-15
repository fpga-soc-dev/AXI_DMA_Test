#include "AxiDmaProxy.hpp"
#include <spdlog/spdlog.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace Dma
{
    AxiDmaProxy::AxiDmaProxy(const DmaProxyChannelConfig &rx_config, const DmaProxyChannelConfig &tx_config)
        : m_RxConfig(std::move(rx_config))
    // m_TxConfig(std::move(tx_config))
    {
        if (m_RxConfig.device_name.empty())
        {
            SPDLOG_ERROR("AxiDmaProxy: Device name cannot be empty");
        }
        if (m_RxConfig.buffer_count == 0 || m_RxConfig.buffer_size == 0)
        {
            SPDLOG_ERROR("AxiDmaProxy: Buffer count must be greater than zero");
        }

        std::string path = "/dev/" + m_RxConfig.device_name;
        m_RxChannel.fd = ::open(path.c_str(), O_RDWR);
        if (m_RxChannel.fd < 0)
        {
            SPDLOG_ERROR("AxiDmaProxy: Failed to open device {}", path);
        }

        const std::size_t map_size = sizeof(ChannelBuffer) * m_RxConfig.buffer_count;
        void *addr = ::mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_RxChannel.fd, 0);
        if (addr == MAP_FAILED)
        {
            SPDLOG_ERROR("AxiDmaProxy: Failed to mmap device memory");
            ::close(m_RxChannel.fd);
            m_RxChannel.fd = -1;
        }
        m_RxChannel.buffer_ptr = static_cast<ChannelBuffer *>(addr);
    }

    AxiDmaProxy::~AxiDmaProxy()
    {
        if (m_RxChannel.buffer_ptr)
        {
            const std::size_t map_size = sizeof(ChannelBuffer) * m_RxConfig.buffer_count;
            ::munmap(m_RxChannel.buffer_ptr, map_size);
            m_RxChannel.buffer_ptr = nullptr;
        }

        if (m_RxChannel.fd >= 0)
        {
            ::close(m_RxChannel.fd);
            m_RxChannel.fd = -1;
        }
    }

    void AxiDmaProxy::RxStop()
    {
        m_Running.store(false, std::memory_order_relaxed);
    }

    void AxiDmaProxy::RxChannelCyclicStart(std::uint32_t bytes_per_transfer, RxCallback cb)
    {
        if (m_RxConfig.direction != DmaProxyDirection::RX)
        {
            SPDLOG_ERROR("AxiDmaProxy: RxChannelStart called on non-RX channel");
            return;
        }

        if (!m_RxChannel.buffer_ptr || m_RxChannel.fd < 0)
        {
            SPDLOG_ERROR("AxiDmaProxy: RX channel not properly initialized");
            return;
        }

        if (bytes_per_transfer == 0 || bytes_per_transfer > m_RxConfig.buffer_size)
        {
            SPDLOG_ERROR("AxiDmaProxy: Invalid bytes_per_transfer: {}", bytes_per_transfer);
            return;
        }

        m_Running.store(true, std::memory_order_relaxed);

        std::uint64_t rx_counter = 0;
        int buffer_index = 0;

        const int count = static_cast<int>(m_RxConfig.buffer_count);
        const int increment = static_cast<int>(m_RxConfig.buffer_increment);

        for (buffer_index = 0; buffer_index < count; buffer_index += increment)
        {
            m_RxChannel.buffer_ptr[buffer_index].length = bytes_per_transfer;
            ::ioctl(m_RxChannel.fd, DMA_PROXY_IOCTL_START_XFER, &buffer_index);
        }

        buffer_index = 0;

        SPDLOG_INFO("AxiDmaProxy: Starting cyclic RX (bytes_per_transfer={})", bytes_per_transfer);

        while (m_Running.load(std::memory_order_relaxed))
        {
            ::ioctl(m_RxChannel.fd, DMA_PROXY_IOCTL_FINISH_XFER, &buffer_index);

            auto &buf = m_RxChannel.buffer_ptr[buffer_index];

            if (buf.status != ProxyStatus::NO_ERROR)
            {
                SPDLOG_ERROR("AxiDmaProxy: RX transfer error on buffer {}: status {}", buffer_index,
                             static_cast<uint32_t>(buf.status));
                break;
            }

            auto *base = reinterpret_cast<std::byte *>(buf.buffer);
            BufferView view{base, bytes_per_transfer};

            if (cb)
            {
                cb(view, rx_counter);
            }

            rx_counter++;

            buf.length = bytes_per_transfer;
            ::ioctl(m_RxChannel.fd, DMA_PROXY_IOCTL_START_XFER, &buffer_index);

            buffer_index += increment;
            buffer_index %= count;
        }
        SPDLOG_INFO("AxiDmaProxy: Cyclic RX loop stopped after {} frames", rx_counter);
    }

} // namespace Dma