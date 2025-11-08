#pragma once

#include <memory>
#include <string>
#include "Uio.hpp"
#include "AxiDmaBdRing.hpp"

namespace Dma
{
    class AxiDma
    {
    public:
        struct Config
        {
            bool HasStsCntrlStrm{false};
            bool HasMm2s{false};
            bool HasMm2sDRE{false};
            int Mm2SDataWidth{0};
            bool HasS2mm{false};
            bool HasS2mmDRE{false};
            int S2MmDataWidth{32};
            bool HasScatterGather{false};
            int Mm2SNumChannels{0};
            int S2MmNumChannels{1};
            int Mm2SBurstSize{0};
            int S2MmBurstSize{256};
            bool MicroDmaChannel{false};
            int AddressWidth{32};
            int BufferLengthRegister{26};
        };

        enum class Direction
        {
            MM2S,
            S2MM
        };

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

        AxiDma(const std::string &uio_device, Config config);
        ~AxiDma();

        void XAxiDmaReset();
        void XAxiDmaIntrDisable(Direction direction, std::uint32_t mask);
        void XAxiDmaIntrEnable(Direction direction, std::uint32_t mask);
        std::uint32_t XAxiDmaIntrGetIrq(Direction direction);
        bool XAxiDmaSelectCyclicMode(Direction direction, bool enable);
        bool XAxiDmaSelectKeyHoleMode(Direction direction, bool enable);
        void XAxiDmaSetBufferAddress(Direction direction, std::uint64_t buffer_address);
        void XAxiDmaStart(Direction direction, bool start);
        void XAxiDmaLength(Direction direction, std::uint32_t length);

        bool XAxiDmaSimpleTransfer(Direction direction, std::uint64_t buffer_address, std::uint32_t length);

        void XAxiDmaWaitSync(Direction direction);

        StatusBits XAxiDmaStatusRead(Direction direction);
        void XAxiDmaPrintStatus(Direction direction);
        ControlBits XAxiDmaControlRead(Direction direction);
        void XAxiDmaPrintControl(Direction direction);

        void XAxiDmaClearInterrupts(Direction direction);

        void Stop();

        void AttachInterruptHandler(Direction direction, const std::function<void()> &callback, bool queue_enabled = false);
        void DetachInterruptHandler();

    private:
        std::shared_ptr<UIO> m_UioDevice;
        std::function<void()> m_S2mmCallback;
        std::function<void()> m_Mm2sCallback;
        Config m_Config;
        bool m_IsInitialized{false};
        XAxiDmaBdRing m_TxBdRing;
        XAxiDmaBdRing m_RxBdRing;

        bool InitializeConfig();
        bool HasMM2S();
        bool HasS2MM();
        bool HasScatterGather();
        bool XAxiDmaResetIsDone();
        bool XAxiDmaBusy(Direction direction);

        void OnS2mmInterrupt();
    };
}