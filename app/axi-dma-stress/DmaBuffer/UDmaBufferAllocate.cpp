#include "UDmaBufferAllocate.hpp"
#include <stdexcept>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <filesystem>

namespace Dma
{

    UDmaBufferAllocate::~UDmaBufferAllocate()
    {
        FreeBuffers();
    }

    bool UDmaBufferAllocate::AllocateBuffers(uint8_t num_buffers, uint32_t size_per_buffer, uint8_t quirk_mmap_mode)
    {
        try
        {
            if (num_buffers < 1 || num_buffers > 8 || size_per_buffer < 1)
            {
                throw std::runtime_error("Buffer number must be between 1 and 8 and size must be at least 1");
            }

            struct utsname uts;
            if (uname(&uts) != 0)
            {
                throw std::runtime_error("Failed to get system information: " + std::string(strerror(errno)));
            }

            std::string module_path = "/lib/modules/" + std::string(uts.release) + "/updates/" + U_DMA_BUF_MODULE_NAME;

            m_ModuleFd = ::open(module_path.c_str(), O_RDONLY);
            if (m_ModuleFd < 0)
            {
                throw std::runtime_error("Failed to open u-dma-buf module file: " + std::string(strerror(errno)));
            }

            std::string param;
            for (int i = 0; i < num_buffers; ++i)
            {
                param += "udmabuf" + std::to_string(i) + "=" + std::to_string(size_per_buffer) + " ";
            }

            param += "quirk_mmap_mode=" + std::to_string(quirk_mmap_mode) + " info_enable=0";

            if (syscall(SYS_finit_module, m_ModuleFd, param.c_str(), 0) != 0)
            {
                if (errno == EEXIST)
                {
                    SPDLOG_WARN("u_dma_buf module already loaded. Now unloading and reloading with new parameters.");
                    if (syscall(SYS_delete_module, U_DMA_BUF_LOADED_MODULE, 0) != 0)
                    {
                        SPDLOG_WARN("Failed to unload u_dma_buf module: {}", strerror(errno));
                    }
                }
                else
                {
                    ::close(m_ModuleFd);
                    m_ModuleFd = -1;
                    throw std::runtime_error("Failed to load u-dma-buf module: " + std::string(strerror(errno)));
                }
            }

            m_AllocatedBuffersCount = num_buffers;
            SPDLOG_INFO("Successfully allocated {} DMA buffers of size {} bytes each.", num_buffers, size_per_buffer);
            return true;
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to allocate buffer: " + std::string(e.what()));
        }
    }

    bool UDmaBufferAllocate::AllocateBuffers(UdmaBufConfig buffer_vec, uint8_t quirk_mmap_mode)
    {
        try
        {
            if (buffer_vec.buffer_vec.size() < 1 || buffer_vec.buffer_vec.size() > 8)
            {
                throw std::runtime_error("Buffer number must be between 1 and 8");
            }

            struct utsname uts;
            if (uname(&uts) != 0)
            {
                throw std::runtime_error("Failed to get system information: " + std::string(strerror(errno)));
            }

            std::string module_path = "/lib/modules/" + std::string(uts.release) + "/updates/" + U_DMA_BUF_MODULE_NAME;

            m_ModuleFd = ::open(module_path.c_str(), O_RDONLY);
            if (m_ModuleFd < 0)
            {
                throw std::runtime_error("Failed to open u-dma-buf module file: " + std::string(strerror(errno)));
            }

            std::string param;
            for (const auto &buf_elem : buffer_vec.buffer_vec)
            {
                param += "udmabuf" + std::to_string(buf_elem.first) + "=" + std::to_string(buf_elem.second) + " ";
            }

            param += "quirk_mmap_mode=" + std::to_string(quirk_mmap_mode) + " info_enable=0";

            if (syscall(SYS_finit_module, m_ModuleFd, param.c_str(), 0) != 0)
            {
                if (errno == EEXIST)
                {
                    SPDLOG_WARN("u_dma_buf module already loaded. Now unloading and reloading with new parameters.");
                    if (syscall(SYS_delete_module, U_DMA_BUF_LOADED_MODULE, 0) != 0)
                    {
                        SPDLOG_WARN("Failed to unload u_dma_buf module: {}", strerror(errno));
                    }
                }
                else
                {
                    ::close(m_ModuleFd);
                    m_ModuleFd = -1;
                    throw std::runtime_error("Failed to load u-dma-buf module: " + std::string(strerror(errno)));
                }
            }

            m_AllocatedBuffersCount = static_cast<uint8_t>(buffer_vec.buffer_vec.size());
            SPDLOG_INFO("Successfully allocated {} DMA buffers", buffer_vec.buffer_vec.size());
            return true;
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Failed to allocate buffer: " + std::string(e.what()));
        }
    }

    bool UDmaBufferAllocate::FreeBuffers()
    {
        if (syscall(SYS_delete_module, U_DMA_BUF_LOADED_MODULE, 0) != 0)
        {
            SPDLOG_WARN("Failed to unload u_dma_buf module: {}", strerror(errno));
            return false;
        }

        if (m_ModuleFd >= 0)
        {
            ::close(m_ModuleFd);
            m_ModuleFd = -1;
            return true;
        }
        return false;
    }

    std::vector<std::string> UDmaBufferAllocate::GetAllocatedBuffers()
    {
        std::filesystem::path class_path(U_DMA_BUF_CLASS_PATH);
        if (!std::filesystem::exists(class_path))
        {
            m_IsAllocated = false;
            m_AllocatedBuffersCount = 0;
            return {};
        }

        std::vector<std::string> buffer_list;
        for (const auto &entry : std::filesystem::directory_iterator(class_path))
        {
            if (entry.is_directory())
            {
                buffer_list.push_back(entry.path().filename().string());
            }
        }

        m_IsAllocated = !buffer_list.empty();
        m_AllocatedBuffersCount = static_cast<uint8_t>(buffer_list.size());

        return buffer_list;
    }
}
