#pragma once

#include "Uio.hpp"
#include <memory>
#include <string>
#include <cstdint>

namespace Dma
{
    class AxiDma
    {
    public:
        static constexpr uint32_t MM2S_CONTROL_REGISTER = 0x00;
        static constexpr uint32_t MM2S_STATUS_REGISTER = 0x04;
        static constexpr uint32_t MM2S_START_ADDRESS = 0x18;
        static constexpr uint32_t MM2S_START_ADDRESS_MSB = 0x1C;
        static constexpr uint32_t MM2S_LENGTH = 0x28;

        static constexpr uint32_t S2MM_CONTROL_REGISTER = 0x30;
        static constexpr uint32_t S2MM_STATUS_REGISTER = 0x34;
        static constexpr uint32_t S2MM_START_ADDRESS = 0x48;
        static constexpr uint32_t S2MM_START_ADDRESS_MSB = 0x4C;
        static constexpr uint32_t S2MM_LENGTH = 0x58;

        struct StatusBits
        {
            bool halted;
            bool idle;
            bool sg_included;
            bool dma_internal_error;
            bool dma_slave_error;
            bool dma_decode_error;
            bool sg_internal_error;
            bool sg_slave_error;
            bool sg_decode_error;
            bool ioc_irq;
            bool dly_irq;
            bool err_irq;
            uint8_t irq_threshold_sts;
            uint8_t irq_delay_sts;
        };

        struct ControlBits
        {
            bool rs;
            bool reset;
            bool keyhole;
            bool cyclic_bd_enable;
            bool ioc_irq_enable;
            bool dly_irq_enable;
            bool err_irq_enable;
            uint8_t irq_threshold;
            uint8_t irq_delay;
        };

        AxiDma(const std::string &uio_device);
        ~AxiDma();

        void ResetMm2s();
        void ResetS2mm();

        void HaltMm2s();
        void HaltS2mm();

        void SetSourceAddress(uint64_t address);
        void SetDestinationAddress(uint64_t address);

        void SetAllInterruptsMaskedMm2s();
        void SetAllInterruptsMaskedS2mm();

        void SetMm2sLength(uint32_t length);
        void SetS2mmLength(uint32_t length);

        void WaitS2mmSync();
        void WaitMm2sSync();

        void ClearAllInterruptsMm2s();
        void ClearAllInterruptsS2mm();

        StatusBits ReadMm2sStatus();
        StatusBits ReadS2mmStatus();
        ControlBits ReadMm2sControl();
        ControlBits ReadS2mmControl();

        void StartS2mm(uint64_t address, uint32_t length);
        void StartMm2s(uint64_t address, uint32_t length);
        void Stop();

        void PrintMm2sStatus();
        void PrintS2mmStatus();
        void PrintMm2sControl();
        void PrintS2mmControl();

        void AttachS2mmInterruptHandler(const std::function<void()> &callback, bool queue_enabled = false);
        void DetachS2mmInterruptHandler();

    private:
        std::unique_ptr<UIO> m_UioDevice;
        std::function<void()> m_S2mmCallback;

        void OnInterruptS2mm();
    };
}