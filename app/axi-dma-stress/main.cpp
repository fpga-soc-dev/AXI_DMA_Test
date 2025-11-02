#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <spdlog/spdlog.h>
#include "UDmaBuffer.hpp"
#include "AxiDataGen.hpp"
#include "AxiDma.hpp"

struct FrameData
{
    uint32_t frame_id;
    std::chrono::steady_clock::time_point timestamp;
};

int main()
{
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%^%l%$] %v");

    try
    {
        AxiDataGen dataGen("data-generator");
        uint32_t version = dataGen.ReadVersion();
        SPDLOG_INFO("AxiDataGen Version: 0x{:08X}", version);
        dataGen.Start(256, 10000u);

        Dma::UDmaBuffer dmaBuffer(1, dataGen.GetFrameSize(), Dma::UDmaBuffer::CachePolicy::Cached);
        Dma::AxiDma axiDma("axi-dma");

        std::atomic<bool> test_running{true};
        std::atomic<int> interrupt_count{0};

        auto buffer_names = dmaBuffer.GetAllocatedBuffers();
        SPDLOG_INFO("Using buffers: {}", buffer_names[0]);

        axiDma.AttachS2mmInterruptHandler([&]()
                                          {
            if (!test_running.load())
                return;
            
            axiDma.ClearAllInterruptsS2mm();
            auto timestamp = std::chrono::steady_clock::now();
            std::string buf_name = buffer_names[0];

            dmaBuffer.InvalidateCache(buf_name);
            std::byte *data_ptr = dmaBuffer.BufferPtr(buf_name);
            uint32_t *frame_data = reinterpret_cast<uint32_t *>(data_ptr);
            
            uint32_t frame_id = frame_data[0];
            SPDLOG_INFO("0: {} 1: {} 254: {} 255: {}", frame_data[0], frame_data[1], frame_data[254], frame_data[255]);
 

            /*               
            for(size_t i = 0; i < 256; i++)
            {
                SPDLOG_INFO("FrameData[{}]: {}", i, frame_data[i]);
            }
            */
            FrameData frame_info;
            frame_info.frame_id = frame_id;
            frame_info.timestamp = timestamp;

            interrupt_count++;

            axiDma.StartS2mm(dmaBuffer.GetPhysicalAddress("udmabuf0"), dmaBuffer.GetBufferSize("udmabuf0")); }, false);

        axiDma.StartS2mm(dmaBuffer.GetPhysicalAddress("udmabuf0"), dmaBuffer.GetBufferSize("udmabuf0"));
        SPDLOG_INFO("Starting 10-second test with single frame transfers...");

        std::this_thread::sleep_for(std::chrono::seconds(10));

        SPDLOG_INFO("Test complete. Total interrupts received: {}", interrupt_count.load());
        test_running.store(false);
        axiDma.DetachS2mmInterruptHandler();
        axiDma.Stop();
        dataGen.Stop();
    }
    catch (const std::exception &e)
    {
        SPDLOG_ERROR("Error: {}", e.what());
    }

    return 0;
}
