#include "Uio.hpp"
#include "EPoll.hpp"
#include "Utils.hpp"
#include <iostream>

UIO::UIO(const std::string &device_name)
    : m_DeviceName(Utils::BuildInstanceName(FindByDeviceName(device_name))),
      m_DevicePath("/dev/" + m_DeviceName),
      m_ClassPath("/sys/class/uio/" + m_DeviceName + "/"),
      m_PhysicalAddress(Utils::ReadFromFile<uint64_t, std::ios_base::hex>(m_ClassPath + "maps/map0/addr")),
      m_AreaSize(Utils::ReadFromFile<uint64_t, std::ios_base::hex>(m_ClassPath + "maps/map0/size")),
      m_Offset(Utils::ReadFromFile<uint64_t, std::ios_base::hex>(m_ClassPath + "maps/map0/offset")),
      m_Fh(m_DevicePath, Utils::OpenMode::ReadWriteSync),
      m_Fm(m_Fh.Handle(), m_AreaSize, 0, Utils::FileMapping::Access::ReadWrite),
      m_AbortRequested(false),
      m_TotalInterrupts(0)
{
}

int UIO::FindByDeviceName(const std::string &device_name)
{
    try
    {
        for (unsigned i = 0; i < 32; ++i)
        {
            std::string path = std::string("/sys/class/uio/uio") + std::to_string(i) + std::string("/name");
            auto devname = Utils::ReadFromFile<std::string>(path);

            if (devname.compare(device_name) == 0)
            {
                return static_cast<int>(i);
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "UIO FindByDeviceName exception: " << e.what() << std::endl;
    }

    return -1;
}

void *UIO::Ptr()
{
    uint8_t *ptr = static_cast<uint8_t *>(m_Fm.Ptr()) + m_Offset;
    return static_cast<void *>(ptr);
}

void UIO::Write(uint32_t device_register, uint32_t value)
{
    uint32_t *baseAddress32 = static_cast<uint32_t *>(Ptr());
    baseAddress32[device_register / sizeof(uint32_t)] = value;
}

uint32_t UIO::Read(uint32_t device_register)
{
    uint32_t *baseAddress32 = static_cast<uint32_t *>(Ptr());
    return baseAddress32[device_register / sizeof(uint32_t)];
}

void UIO::Write64(uint32_t device_register, uint64_t value)
{
    uint64_t *baseAddress64 = static_cast<uint64_t *>(Ptr());
    baseAddress64[device_register / sizeof(uint64_t)] = value;
}

uint64_t UIO::Read64(uint32_t device_register)
{
    uint64_t *baseAddress64 = static_cast<uint64_t *>(Ptr());
    return baseAddress64[device_register / sizeof(uint64_t)];
}

uint64_t UIO::GetPhysicalAddress() const
{
    return m_PhysicalAddress;
}

std::size_t UIO::GetAreaSize() const
{
    return m_AreaSize;
}

UIO::~UIO()
{
    InterruptDetach();
}

void UIO::InterruptAttach(const std::function<void()> &callback, bool queue_enabled)
{
    {
        std::lock_guard<std::mutex> lock(m_CallbackMutex);
        if (m_Callback)
        {
            throw std::runtime_error("Interrupt already attached");
        }
        m_Callback = callback;
    }

    m_QueueEnabled.store(queue_enabled);

    if (m_QueueEnabled.load())
    {
        m_InterruptCounterTh = std::thread(&UIO::InterruptCounterQueueTh, this);
        m_CallbackTh = std::thread(&UIO::CallbackWithQueueTh, this);
    }
    else
    {
        m_CallbackTh = std::thread(&UIO::CallbackWithoutQueueTh, this);
    }
}

void UIO::InterruptDetach()
{
    m_Pipe.Write("q", 1);

    if (m_QueueEnabled.load())
    {
        if (m_InterruptCounterTh.joinable())
        {
            m_InterruptCounterTh.join();
        }
    }

    if (m_CallbackTh.joinable())
    {
        m_CallbackTh.join();
    }

    {
        std::lock_guard<std::mutex> lock(m_CallbackMutex);
        m_Callback = nullptr;
    }
}

void UIO::CallbackWithoutQueueTh()
{
    volatile bool abortRequested = false;
    Utils::EPoll epoll;

    epoll.Add(m_Pipe.ReadHandle(), Utils::EPoll::TriggerOP::Read, [&abortRequested](int)
              { abortRequested = true; });

    epoll.Add(m_Fh.Handle(), Utils::EPoll::TriggerOP::Read, [this](int)
              {
                    uint32_t irq_count;
                    m_Fh.Read(&irq_count, sizeof(irq_count));
                    std::function<void()> callback;
                    {
                        std::lock_guard<std::mutex> lock(m_CallbackMutex);
                        callback = m_Callback;
                    }
  
                    if (callback)
                    {
                        callback();
                    }

                    irq_count=1;
                    m_Fh.Write(&irq_count,sizeof(irq_count)); });

    try
    {
        {
            uint32_t enable{1};
            m_Fh.Write(&enable, sizeof(enable));
        }
        while (!abortRequested)
        {
            epoll.Poll();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "UIO interrupt thread exception: " << e.what() << std::endl;
    }
}

void UIO::InterruptCounterQueueTh()
{
    volatile bool abortRequested{false};
    Utils::EPoll poll;

    poll.Add(m_Pipe.ReadHandle(), Utils::EPoll::TriggerOP::Read, [this, &abortRequested](int)
             {  abortRequested = true;
                    m_AbortRequested.store(true); 
                    m_Queuecv.notify_all(); });

    poll.Add(m_Fh.Handle(), Utils::EPoll::TriggerOP::Read, [this](int)
             {
                    uint32_t irq_count{0};
                    m_Fh.Read(&irq_count, sizeof(irq_count));
                    {
                        std::lock_guard<std::mutex> lock(m_QueueMutex);
                        m_InterruptQueue.push(irq_count);
                    }
                    m_Queuecv.notify_all();
                    m_TotalInterrupts++;
                    irq_count = 1;
                    m_Fh.Write(&irq_count, sizeof(irq_count)); });

    try
    {
        {
            uint32_t enable{1};
            m_Fh.Write(&enable, sizeof(enable));
        }
        while (!abortRequested)
        {
            poll.Poll();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "UIO interrupt counter thread exception: " << e.what() << std::endl;
    }
}

void UIO::CallbackWithQueueTh()
{

    while (!m_AbortRequested.load())
    {
        std::unique_lock<std::mutex> lock(m_QueueMutex);
        m_Queuecv.wait(lock, [this]
                       { return !m_InterruptQueue.empty() || m_AbortRequested.load(); });

        while (!m_InterruptQueue.empty())
        {
            {
                std::lock_guard<std::mutex> lock(m_QueueMutex);
                uint32_t irq_count = m_InterruptQueue.front();
                m_InterruptQueue.pop();
            }

            std::function<void()> callback;
            {
                std::lock_guard<std::mutex> cb_lock(m_CallbackMutex);
                callback = m_Callback;
            }

            if (callback)
            {
                callback();
            }
        }
    }
}

void UIO::InterruptAttachByTimer(const std::function<void()> &callback, std::chrono::milliseconds duration, bool queue_enabled)
{
    m_QueueEnabled = queue_enabled;
    if (m_QueueEnabled)
    {
        m_InterruptCounterTh = std::thread(&UIO::InterruptCounterQueueTh, this);
        m_CallbackTh = std::thread(&UIO::CallbackWithQueueTh, this);
    }
    else
    {
        m_CallbackTh = std::thread(&UIO::CallbackWithoutQueueTh, this);
    }

    std::thread([this, duration]()
                {
                        std::this_thread::sleep_for(duration);
                        this->InterruptDetach(); })
        .detach();
}

uint32_t UIO::GetTotalInterrupts() const
{
    return m_TotalInterrupts.load();
}

void UIO::WaitForInterrupt()
{
    unsigned int dummy;
    m_Fh.Write((char *)&IRQ_RST, sizeof(IRQ_RST));
    m_Fh.Read((char *)&dummy, sizeof(dummy));
}
