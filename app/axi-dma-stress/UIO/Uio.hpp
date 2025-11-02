#pragma once

#include <functional>
#include <string>
#include <string_view>
#include "FileHandle.hpp"
#include "FileMapping.hpp"
#include "Utils.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>

class UIO
{
    static constexpr int IRQ_RST = 1;

public:
    explicit UIO(const std::string &device_name);
    UIO(const UIO &) = delete;
    UIO &operator=(const UIO &) = delete;

    UIO(UIO &&other) noexcept = default;
    UIO &operator=(UIO &&other) noexcept = default;
    ~UIO();

    void Write(uint32_t device_register, uint32_t value);
    uint32_t Read(uint32_t device_register);
    void Write64(uint32_t device_register, uint64_t value);
    uint64_t Read64(uint32_t device_register);
    void InterruptAttach(const std::function<void()> &callback, bool queue_enabled = false);
    void InterruptDetach();
    void InterruptAttachByTimer(const std::function<void()> &callback, std::chrono::milliseconds duration, bool queue_enabled = false);

    void WaitForInterrupt();

    uint64_t GetPhysicalAddress() const;
    std::size_t GetAreaSize() const;
    void *Ptr();
    uint32_t GetTotalInterrupts() const;

private:
    void CallbackWithQueueTh();
    void InterruptCounterQueueTh();
    void CallbackWithoutQueueTh();
    int FindByDeviceName(const std::string &device_name);

    const std::string m_DeviceName;
    const std::string m_DevicePath;
    const std::string m_ClassPath;

    const uint64_t m_PhysicalAddress;
    const std::size_t m_AreaSize;
    const off_t m_Offset;

    Utils::FileHandle m_Fh;
    Utils::FileMapping m_Fm;
    Utils::Pipe m_Pipe;

    std::thread m_InterruptCounterTh, m_CallbackTh;
    std::atomic<bool> m_AbortRequested, m_QueueEnabled;
    std::atomic<uint32_t> m_TotalInterrupts;
    std::function<void()> m_Callback;

    std::queue<uint32_t> m_InterruptQueue;
    std::mutex m_QueueMutex, m_CallbackMutex;
    std::condition_variable m_Queuecv;
};
