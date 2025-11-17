#include "AxiDataGen.hpp"
#include <spdlog/spdlog.h>

AxiDataGen::AxiDataGen(const std::string &uio_device) : m_UioDevice(std::make_unique<UIO>(uio_device))
{
}

AxiDataGen::~AxiDataGen()
{
    Stop();
    SoftReset();
}

uint32_t AxiDataGen::ReadVersion()
{
    return m_UioDevice->Read(VERSION);
}

void AxiDataGen::SetConfig(uint32_t frame_samples, uint32_t frame_period)
{

    uint32_t desired_interval_us = frame_period;
    uint32_t calculated_period = ComputeSamplePeriod(desired_interval_us,
                                                     DATA_GENERATOR_FREQ,
                                                     frame_samples);

    m_UioDevice->Write(SAMPLE_PERIOD, calculated_period);
    if (m_UioDevice->Read(SAMPLE_PERIOD) != calculated_period)
    {
        SPDLOG_WARN("SAMPLE_PERIOD register mismatch after write {}", calculated_period);
    }

    m_FramePeriod = calculated_period;

    m_UioDevice->Write(FRAME_SAMPLES, frame_samples);
    auto frames_samples_readback = m_UioDevice->Read(FRAME_SAMPLES);
    if (frames_samples_readback != frame_samples)
    {
        SPDLOG_WARN("FRAME_SAMPLES register mismatch after write {} (read back {})", frame_samples, frames_samples_readback);
    }
    m_FrameSamples = frame_samples;
}

void AxiDataGen::Start()
{
    auto version = ReadVersion();
    if (version != 0x0000DEAD)
    {
        SPDLOG_WARN("Unexpected AxiDataGen version: 0x{:08X}", version);
    }

    ClearBackPressureCount();
    ClearIntervalDone();
    ClearInterruptCount();
    ClearAverageInterval();
    SoftReset();

    m_UioDevice->Write(STATUS_CLR, 0x0);
    m_UioDevice->Write(FRAME_ID, 0x0);
    if (m_UioDevice->Read(FRAME_ID) != 0x0)
    {
        SPDLOG_WARN("FRAME_ID register mismatch after write");
    }

    m_UioDevice->Write(CONTROL, CONTROL_ENABLE_BIT | CONTROL_ARM_INTERVAL_BIT);
}

uint32_t AxiDataGen::ComputeSamplePeriod(uint32_t interval_us,
                                         uint32_t axi_clock_freq,
                                         uint32_t frame_samples)
{
    const uint64_t total_target_clocks = (static_cast<uint64_t>(interval_us) *
                                          static_cast<uint64_t>(axi_clock_freq)) /
                                         1'000'000u;

    const uint64_t data_transfer_clocks = static_cast<uint64_t>(frame_samples);

    if (total_target_clocks <= data_transfer_clocks)
    {
        SPDLOG_WARN("Requested interval too small to accommodate data transfer. Setting SAMPLE_PERIOD to 0.");
        return 0;
    }
    else
    {
        uint64_t gap_clocks = total_target_clocks - data_transfer_clocks;

        if (gap_clocks > 0xFFFFFFFFull)
        {
            gap_clocks = 0xFFFFFFFFull;
        }
        SPDLOG_DEBUG("Computed SAMPLE_PERIOD: interval_us={}, total_target_clocks={}, data_transfer_clocks={}, gap_clocks={}",
                     interval_us,
                     total_target_clocks,
                     data_transfer_clocks,
                     gap_clocks);
        return static_cast<uint32_t>(gap_clocks);
    }
}

uint32_t AxiDataGen::GetFrameSize()
{
    return m_UioDevice->Read(FRAME_SAMPLES) * AXI_DATA_SIZE_BYTES;
}

uint32_t AxiDataGen::GetBackPressureCount()
{
    return (m_UioDevice->Read(STATUS) >> 16) & 0xFFFF;
}

uint32_t AxiDataGen::GetInterruptCount()
{
    return m_UioDevice->Read(IRQ_COUNT);
}

void AxiDataGen::ClearBackPressureCount()
{
    uint32_t status = m_UioDevice->Read(STATUS_CLR);
    m_UioDevice->Write(STATUS_CLR, status | 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    m_UioDevice->Write(STATUS_CLR, status & ~0x1);

    if (m_UioDevice->Read(STATUS_CLR) & 0x1)
    {
        SPDLOG_WARN("BACKPRESSURE_COUNT register not cleared after ClearBackPressureCount");
    }
}

void AxiDataGen::ClearIntervalDone()
{
    uint32_t status = m_UioDevice->Read(STATUS_CLR);
    m_UioDevice->Write(STATUS_CLR, status | 0x2);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    m_UioDevice->Write(STATUS_CLR, status & ~0x2);

    if (m_UioDevice->Read(STATUS_CLR) & 0x2)
    {
        SPDLOG_WARN("INTERVAL_DONE register not cleared after ClearIntervalDone");
    }
}

void AxiDataGen::ClearInterruptCount()
{
    uint32_t status = m_UioDevice->Read(STATUS_CLR);
    m_UioDevice->Write(STATUS_CLR, status | 0x4);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    m_UioDevice->Write(STATUS_CLR, status & ~0x4);

    if (m_UioDevice->Read(STATUS_CLR) & 0x4)
    {
        SPDLOG_WARN("IRQ_COUNT register not cleared after ClearInterruptCount");
    }
}

void AxiDataGen::ClearAverageInterval()
{
    uint32_t status = m_UioDevice->Read(STATUS_CLR);
    m_UioDevice->Write(STATUS_CLR, status | 0x8);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    m_UioDevice->Write(STATUS_CLR, status & ~0x8);

    if (m_UioDevice->Read(STATUS_CLR) & 0x8)
    {
        SPDLOG_WARN("AVERAGE_INTERVAL register not cleared after ClearAverageInterval");
    }
}

void AxiDataGen::SoftReset()
{
    uint32_t control = m_UioDevice->Read(CONTROL);

    m_UioDevice->Write(CONTROL, control | CONTROL_SOFT_RESET_BIT);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    m_UioDevice->Write(CONTROL, control & ~CONTROL_SOFT_RESET_BIT);
}

double AxiDataGen::GetIntervalResultMs()
{
    return (m_UioDevice->Read(INTERVAL_RESULT) * 1000.0 / DATA_GENERATOR_FREQ);
}

void AxiDataGen::Stop()
{
    uint32_t control_reg = m_UioDevice->Read(CONTROL);
    control_reg &= ~CONTROL_ENABLE_BIT;
    m_UioDevice->Write(CONTROL, control_reg);
}
