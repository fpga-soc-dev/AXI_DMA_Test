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

    bool UDmaBufferAllocate::AllocateBuffers(uint8_t num_buffers,
                                             uint32_t size_per_buffer,
                                             uint8_t quirk_mmap_mode)
    {

        if (num_buffers < 1 || num_buffers > 8 || size_per_buffer < 1)
        {
            SPDLOG_ERROR("Invalid buffer parameters: count={} size={}", num_buffers, size_per_buffer);
            return false;
        }

        struct utsname uts{};
        if (uname(&uts) != 0)
        {
            SPDLOG_ERROR("Failed to get system information: {}", strerror(errno));
            return false;
        }

        std::string module_path = "/lib/modules/" + std::string(uts.release) + "/updates/" + std::string(U_DMA_BUF_MODULE_NAME);
        if (!std::filesystem::exists(module_path))
        {
            std::string alt = "/lib/modules/" + std::string(uts.release) + "/extra/" + std::string(U_DMA_BUF_MODULE_NAME);
            if (std::filesystem::exists(alt))
            {
                module_path = alt;
            }
        }

        m_ModuleFd = ::open(module_path.c_str(), O_RDONLY);
        if (m_ModuleFd < 0)
        {
            SPDLOG_ERROR("Failed to open u-dma-buf module file '{}': {}", module_path, strerror(errno));
            return false;
        }

        std::string param;
        for (uint8_t i = 0; i < num_buffers; ++i)
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
                    SPDLOG_ERROR("Failed to unload existing u_dma_buf module: {}", strerror(errno));
                    ::close(m_ModuleFd);
                    m_ModuleFd = -1;
                    return false;
                }

                if (syscall(SYS_finit_module, m_ModuleFd, param.c_str(), 0) != 0)
                {
                    SPDLOG_ERROR("Failed to reload u_dma_buf module with new params: {}", strerror(errno));
                    ::close(m_ModuleFd);
                    m_ModuleFd = -1;
                    return false;
                }
            }
            else
            {
                ::close(m_ModuleFd);
                m_ModuleFd = -1;
                SPDLOG_ERROR("Failed to load u-dma-buf module: {}", strerror(errno));
                return false;
            }
        }

        m_AllocatedBuffersCount = num_buffers;
        m_IsAllocated = true;
        SPDLOG_INFO("Successfully allocated {} DMA buffers of size {} bytes each.", num_buffers, size_per_buffer);
        return true;
    }

    bool UDmaBufferAllocate::AllocateBuffers(UdmaBufConfig cfg, uint8_t quirk_mmap_mode)
    {

        if (cfg.buffer_vec.size() < 1 || cfg.buffer_vec.size() > 8)
        {
            SPDLOG_ERROR("Buffer number must be between 1 and 8 (got {})", cfg.buffer_vec.size());
            return false;
        }

        struct utsname uts{};
        if (uname(&uts) != 0)
        {
            SPDLOG_ERROR("Failed to get system information: {}", strerror(errno));
            return false;
        }

        std::string module_path = "/lib/modules/" + std::string(uts.release) + "/updates/" + std::string(U_DMA_BUF_MODULE_NAME);
        if (!std::filesystem::exists(module_path))
        {
            std::string alt =
                "/lib/modules/" + std::string(uts.release) + "/extra/" + std::string(U_DMA_BUF_MODULE_NAME);
            if (std::filesystem::exists(alt))
            {
                module_path = alt;
            }
        }

        m_ModuleFd = ::open(module_path.c_str(), O_RDONLY);
        if (m_ModuleFd < 0)
        {
            SPDLOG_ERROR("Failed to open u-dma-buf module file '{}': {}", module_path, strerror(errno));
            return false;
        }

        std::string param;
        for (const auto &buf_elem : cfg.buffer_vec)
        {
            param += "udmabuf" + std::to_string(buf_elem.index) + "=" + std::to_string(buf_elem.size) + " ";
        }

        param += "quirk_mmap_mode=" + std::to_string(quirk_mmap_mode) + " info_enable=0";

        if (syscall(SYS_finit_module, m_ModuleFd, param.c_str(), 0) != 0)
        {
            if (errno == EEXIST)
            {
                SPDLOG_WARN("u_dma_buf module already loaded, unloading and retrying...");
                if (syscall(SYS_delete_module, U_DMA_BUF_LOADED_MODULE, 0) != 0)
                {
                    SPDLOG_ERROR("Failed to unload existing u_dma_buf module: {}", strerror(errno));
                    ::close(m_ModuleFd);
                    m_ModuleFd = -1;
                    return false;
                }

                if (syscall(SYS_finit_module, m_ModuleFd, param.c_str(), 0) != 0)
                {
                    SPDLOG_ERROR("Failed to reload u_dma_buf module with new params: {}", strerror(errno));
                    ::close(m_ModuleFd);
                    m_ModuleFd = -1;
                    return false;
                }
            }
            else
            {
                SPDLOG_ERROR("Failed to load u-dma-buf module: {}", strerror(errno));
                ::close(m_ModuleFd);
                m_ModuleFd = -1;
                return false;
            }
        }

        m_AllocatedBuffersCount = static_cast<std::uint8_t>(cfg.buffer_vec.size());
        SPDLOG_INFO("Successfully allocated {} DMA buffers", cfg.buffer_vec.size());
        return true;
    }

    bool UDmaBufferAllocate::FreeBuffers()
    {
        if (syscall(SYS_delete_module, U_DMA_BUF_LOADED_MODULE, 0) != 0)
        {
            SPDLOG_WARN("Failed to unload u_dma_buf module: {}", strerror(errno));
        }

        bool closed = true;
        if (m_ModuleFd >= 0)
        {
            ::close(m_ModuleFd);
            m_ModuleFd = -1;
        }
        else
        {
            closed = false;
        }

        m_IsAllocated = false;
        m_AllocatedBuffersCount = 0;

        return closed;
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
