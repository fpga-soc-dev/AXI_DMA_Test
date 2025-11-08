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

void AxiDataGen::Start(uint32_t frame_samples, uint32_t interval_us)
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

    uint32_t desired_interval_us = interval_us;
    uint32_t calculated_period = ComputeSamplePeriod(desired_interval_us,
                                                     DATA_GENERATOR_FREQ,
                                                     frame_samples);

    SPDLOG_INFO("Configuring for {} samples per frame and calculated clock cycles per data word {} for {} us frame period", frame_samples, calculated_period + 1, desired_interval_us);

    m_UioDevice->Write(SAMPLE_PERIOD, calculated_period);
    if (m_UioDevice->Read(SAMPLE_PERIOD) != calculated_period)
    {
        SPDLOG_WARN("SAMPLE_PERIOD register mismatch after write");
    }

    m_UioDevice->Write(FRAME_SAMPLES, frame_samples);
    if (m_UioDevice->Read(FRAME_SAMPLES) != frame_samples)
    {
        SPDLOG_WARN("FRAME_SAMPLES register mismatch after write {}", frame_samples);
    }

    m_UioDevice->Write(FRAME_ID, 0x0);
    if (m_UioDevice->Read(FRAME_ID) != 0x0)
    {
        SPDLOG_WARN("FRAME_ID register mismatch after write");
    }

    m_UioDevice->Write(CONTROL, CONTROL_ENABLE_BIT | CONTROL_ARM_INTERVAL_BIT);
}

uint32_t AxiDataGen::ComputeSamplePeriod(uint32_t interval_us, uint32_t axi_clock_freq, uint32_t frame_samples)
{

    // First, calculate the total number of clock cycles for the entire frame
    // (interval_us * axi_clock_freq) / 1,000,000
    const uint64_t total_clocks = (static_cast<uint64_t>(interval_us) *
                                   static_cast<uint64_t>(axi_clock_freq)) /
                                  1'000'000u;

    // Now, divide total clocks by the number of samples to find
    // how many clocks each sample gets (this is P+1)
    // We add (frame_samples / 2) for rounding.
    uint64_t clocks_per_sample = (total_clocks + frame_samples / 2u) / frame_samples;

    // The minimum (P+1) can be is 1 (which means P=0).
    // If the interval is too fast, clocks_per_sample could be 0.
    if (clocks_per_sample < 1ull)
    {
        clocks_per_sample = 1ull; // Force to minimum (P=0)
    }

    // P = (P+1) - 1
    uint64_t sample_period = clocks_per_sample - 1;

    // Clamp to the max 32-bit register size
    if (sample_period > 0xFFFFFFFFull)
    {
        sample_period = 0xFFFFFFFFull;
    }

    return static_cast<uint32_t>(sample_period);
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
