#pragma once

#include <cstdint>
#include <string>
#include "Uio.hpp"

class AxiDataGen
{
public:
    static constexpr uint32_t CONTROL = 0x00;
    static constexpr uint32_t SAMPLE_PERIOD = 0x04;
    static constexpr uint32_t FRAME_SAMPLES = 0x08;
    static constexpr uint32_t FRAME_ID = 0x0C;
    static constexpr uint32_t STATUS = 0x10;
    static constexpr uint32_t STATUS_CLR = 0x14;
    static constexpr uint32_t VERSION = 0x18;
    static constexpr uint32_t INTERVAL_RESULT = 0x1C;

    static constexpr uint32_t DATA_GENERATOR_FREQ = 200'000'000; // 200 MHz
    static constexpr uint32_t AXI_DATA_SIZE_BYTES = 4;           // 32-bit data

    static constexpr uint32_t CONTROL_ENABLE_BIT = (1u << 0);
    static constexpr uint32_t CONTROL_SOFT_RESET_BIT = (1u << 1);
    static constexpr uint32_t CONTROL_ARM_INTERVAL_BIT = (1u << 2);

    static constexpr uint32_t STATUS_RUNNING_BIT = (1u << 0);
    static constexpr uint32_t STATUS_BACKPRESSURE_SEEN_BIT = (1u << 1);

public:
    explicit AxiDataGen(const std::string &uio_device);

    AxiDataGen(const AxiDataGen &) = delete;
    AxiDataGen &operator=(const AxiDataGen &) = delete;
    AxiDataGen(AxiDataGen &&other) = default;
    AxiDataGen &operator=(AxiDataGen &&other) = default;
    ~AxiDataGen();

    uint32_t ReadVersion();
    void Start(uint32_t frame_samples, uint32_t interval_us);
    uint32_t GetFrameSize();
    uint32_t GetBackPressureCount();
    double GetIntervalResultMs();
    void StatusClear();
    void SoftReset();
    void Stop();

private:
    std::unique_ptr<UIO> m_UioDevice;

    uint32_t ComputeSamplePeriod(uint32_t interval_us, uint32_t axi_clock_freq, uint32_t frame_samples);
};
