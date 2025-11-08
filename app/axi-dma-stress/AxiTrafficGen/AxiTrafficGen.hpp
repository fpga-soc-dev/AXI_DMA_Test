#pragma once

#include <cstdint>
#include <string>
#include "Uio.hpp"

class AxiTrafficGen
{
public:
    static constexpr uint32_t CONTROL = 0x30;
    static constexpr uint32_t CONFIG = 0x34;
    static constexpr uint32_t TRANSFER_LENGTH = 0x38;
    static constexpr uint32_t TRANSFER_COUNT = 0x3C;
    static constexpr uint32_t USER_STRB_1 = 0x40;
    static constexpr uint32_t USER_STRB_2 = 0x44;
    static constexpr uint32_t USER_STRB_3 = 0x48;
    static constexpr uint32_t USER_STRB_4 = 0x4C;
    static constexpr uint32_t EXTENDED_TRANSFER_LENGTH = 0x50;
    static constexpr uint32_t STREAMING_ERROR_STATUS = 0x70;
    static constexpr uint32_t STREAMING_ERROR_ENABLE = 0x74;
    static constexpr uint32_t STREAMING_ERROR_INTERRUPT = 0x78;
    static constexpr uint32_t STREAMING_ERROR_COUNT_REGISTER = 0x7C;

    static constexpr uint8_t VERSION = 0x20;

    AxiTrafficGen(const std::string &uio_device);

    uint8_t ReadVersion();
    void EnableStream();
    void DisableStream();
    bool CheckDone();
    void ClearDone();
    void SetProgrammableDelay(uint32_t delay_cycles);
    void SetTDest(uint8_t tdest);
    void SetEtkts(bool etkt);
    void SetRandomDelay(bool enable);
    void SetRandomLength(bool enable);
    void SetTransactionCount(uint32_t count);
    void SetTransferLength(uint32_t length);
    uint32_t GetTransferCount();
    uint32_t GetStreamingErrorStatus();
    void SetStreamingErrorEnable(bool enable);
    void SetStreamingErrorInterruptEnable(bool enable);
    uint32_t GetStreamingErrorCount();

    void StartStream(uint32_t delay_between_packets, uint32_t value, uint32_t transaction_length, uint32_t transaction_count = 0, bool enable_error = true);
    void StopStream();

private:
    std::unique_ptr<UIO> m_UioDevice;
};