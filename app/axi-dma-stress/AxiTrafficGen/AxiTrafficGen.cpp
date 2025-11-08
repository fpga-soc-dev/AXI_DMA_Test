#include "AxiTrafficGen.hpp"

#include <spdlog/spdlog.h>

AxiTrafficGen::AxiTrafficGen(const std::string &uio_device)
    : m_UioDevice(std::make_unique<UIO>(uio_device))
{
}

void AxiTrafficGen::StartStream(uint32_t delay_between_packets, uint32_t value, uint32_t transaction_length, uint32_t transaction_count, bool enable_error)
{
    auto version = ReadVersion();
    if (version != VERSION)
    {
        SPDLOG_ERROR("Unsupported AxiTrafficGen version: 0x{:02X}", version);
        return;
    }

    SetProgrammableDelay(delay_between_packets);
    SetRandomDelay(false);
    SetRandomLength(false);
    SetTransferLength(transaction_length);
    SetTransactionCount(transaction_count);
    SetStreamingErrorEnable(enable_error);
    EnableStream();
}

void AxiTrafficGen::StopStream()
{
    SPDLOG_INFO("The count of TLAST on Master Interface: {}", GetTransferCount());
    DisableStream();
}

uint8_t AxiTrafficGen::ReadVersion()
{
    return (m_UioDevice->Read(CONTROL) >> 24) & 0xFF;
}

void AxiTrafficGen::EnableStream()
{
    m_UioDevice->Write(CONTROL, 0x1);
}

void AxiTrafficGen::DisableStream()
{
    m_UioDevice->Write(CONTROL, 0x0);
}

bool AxiTrafficGen::CheckDone()
{
    uint32_t status = m_UioDevice->Read(CONTROL);
    return (status & 0x2) != 0;
}

void AxiTrafficGen::ClearDone()
{
    m_UioDevice->Write(CONTROL, m_UioDevice->Read(CONTROL) & ~0x2);
}

void AxiTrafficGen::SetProgrammableDelay(uint32_t delay_cycles)
{
    m_UioDevice->Write(CONFIG, (m_UioDevice->Read(CONFIG) & 0x0000FFFF) | (delay_cycles << 16));
}

void AxiTrafficGen::SetTDest(uint8_t tdest)
{
    m_UioDevice->Write(CONFIG, (m_UioDevice->Read(CONFIG) & 0xFFFF00FF) | ((tdest & 0xFF) << 8));
}

void AxiTrafficGen::SetEtkts(bool etkt)
{
    m_UioDevice->Write(CONFIG, (m_UioDevice->Read(CONFIG) & 0xFFFFFFFB) | ((etkt ? 0x1 : 0x0) << 2));
}

void AxiTrafficGen::SetRandomDelay(bool enable)
{
    m_UioDevice->Write(CONFIG, (m_UioDevice->Read(CONFIG) & 0xFFFFFFFD) | ((enable ? 0x1 : 0x0) << 1));
}

void AxiTrafficGen::SetRandomLength(bool enable)
{
    m_UioDevice->Write(CONFIG, (m_UioDevice->Read(CONFIG) & 0xFFFFFFFE) | (enable ? 0x1 : 0x0));
}

void AxiTrafficGen::SetTransactionCount(uint32_t count)
{
    m_UioDevice->Write(TRANSFER_LENGTH, (m_UioDevice->Read(TRANSFER_LENGTH) & 0x0000FFFF) | (count << 16));
}

void AxiTrafficGen::SetTransferLength(uint32_t length)
{
    m_UioDevice->Write(TRANSFER_LENGTH, (m_UioDevice->Read(TRANSFER_LENGTH) & 0xFFFF0000) | (length & 0xFFFF));
}

uint32_t AxiTrafficGen::GetTransferCount()
{
    return m_UioDevice->Read(TRANSFER_COUNT);
}

uint32_t AxiTrafficGen::GetStreamingErrorStatus()
{
    return m_UioDevice->Read(STREAMING_ERROR_STATUS);
}

void AxiTrafficGen::SetStreamingErrorEnable(bool enable)
{
    m_UioDevice->Write(STREAMING_ERROR_ENABLE, enable ? 0x1 : 0x0);
}

void AxiTrafficGen::SetStreamingErrorInterruptEnable(bool enable)
{
    m_UioDevice->Write(STREAMING_ERROR_INTERRUPT, enable ? 0x1 : 0x0);
}

uint32_t AxiTrafficGen::GetStreamingErrorCount()
{
    return m_UioDevice->Read(STREAMING_ERROR_COUNT_REGISTER);
}
