#include "UDmaBufferAllocate.hpp"
#include <stdexcept>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <filesystem>

namespace Dma
{
    UDmaBufferAllocate &UDmaBufferAllocate::GetInstance()
    {
        static UDmaBufferAllocate instance;
        return instance;
    }

    UDmaBufferAllocate::~UDmaBufferAllocate()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_AllAllocatedBuffers.empty())
        {
            SPDLOG_DEBUG("Clearing buffer tracking, module unload will handle cleanup");
            m_AllAllocatedBuffers.clear();
        }

        if (m_IsModuleLoaded)
        {
            UnloadModule();
        }
    }

    bool UDmaBufferAllocate::LoadModule()
    {
        if (m_IsModuleLoaded)
        {
            return true;
        }

        std::string module_path = GetModulePath();
        if (module_path.empty())
        {
            return false;
        }

        m_ModuleFd = ::open(module_path.c_str(), O_RDONLY);
        if (m_ModuleFd < 0)
        {
            SPDLOG_ERROR("Cannot open module file {}: {}", module_path, strerror(errno));
            return false;
        }

        if (syscall(SYS_finit_module, m_ModuleFd, "", 0) != 0)
        {
            if (errno == EEXIST)
            {
                SPDLOG_DEBUG("u_dma_buf module already loaded");
                m_IsModuleLoaded = true;
                ::close(m_ModuleFd);
                m_ModuleFd = -1;
                return true;
            }
            else
            {
                SPDLOG_ERROR("Failed to load u_dma_buf module: {}", strerror(errno));
                ::close(m_ModuleFd);
                m_ModuleFd = -1;
                return false;
            }
        }

        ::close(m_ModuleFd);
        m_ModuleFd = -1;
        m_IsModuleLoaded = true;
        return true;
    }

    bool UDmaBufferAllocate::UnloadModule()
    {
        if (!m_IsModuleLoaded)
        {
            return true;
        }

        if (syscall(SYS_delete_module, U_DMA_BUF_LOADED_MODULE.data(), 0) != 0)
        {
            if (errno != ENOENT && errno != EFAULT)
            {
                SPDLOG_WARN("Failed to unload u_dma_buf module: {}", strerror(errno));
                return false;
            }
            else
            {
                SPDLOG_DEBUG("u_dma_buf module already unloaded");
            }
        }

        m_IsModuleLoaded = false;
        return true;
    }

    bool UDmaBufferAllocate::AllocateBuffers(uint8_t num_buffers, uint32_t size_per_buffer, uint8_t quirk_mmap_mode)
    {
        if (num_buffers < 1 || num_buffers > 8 || size_per_buffer < 1)
        {
            SPDLOG_ERROR("Invalid buffer parameters: count={} size={}", num_buffers, size_per_buffer);
            return false;
        }

        UdmaBufConfig config;
        for (uint8_t i = 0; i < num_buffers; ++i)
        {
            config.buffer_vec.push_back({i, size_per_buffer});
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        return AllocateBuffersImpl(config, quirk_mmap_mode);
    }

    bool UDmaBufferAllocate::AllocateBuffers(UdmaBufConfig cfg, uint8_t quirk_mmap_mode)
    {
        if (cfg.buffer_vec.size() < 1 || cfg.buffer_vec.size() > 8)
        {
            SPDLOG_ERROR("Buffer number must be between 1 and 8 (got {})", cfg.buffer_vec.size());
            return false;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        return AllocateBuffersImpl(cfg, quirk_mmap_mode);
    }

    bool UDmaBufferAllocate::AllocateBuffersImpl(const UdmaBufConfig &cfg, uint8_t quirk_mmap_mode)
    {
        std::string module_path = GetModulePath();
        if (module_path.empty())
        {
            return false;
        }

        m_ModuleFd = ::open(module_path.c_str(), O_RDONLY);
        if (m_ModuleFd < 0)
        {
            SPDLOG_ERROR("Failed to open u-dma-buf module file '{}': {}", module_path, strerror(errno));
            return false;
        }

        std::string param;
        std::vector<std::string> requested_buffers;

        for (const auto &buf_elem : cfg.buffer_vec)
        {
            param += "udmabuf" + std::to_string(buf_elem.index) + "=" + std::to_string(buf_elem.size) + " ";
            requested_buffers.push_back("udmabuf" + std::to_string(buf_elem.index));
        }

        param += "quirk_mmap_mode=" + std::to_string(quirk_mmap_mode) + " info_enable=0";

        if (syscall(SYS_finit_module, m_ModuleFd, param.c_str(), 0) != 0)
        {
            if (errno == EEXIST)
            {
                SPDLOG_WARN("u_dma_buf module already loaded, unloading and retrying...");

                if (syscall(SYS_delete_module, U_DMA_BUF_LOADED_MODULE.data(), 0) != 0)
                {
                    SPDLOG_ERROR("Failed to unload existing u_dma_buf module: {}", strerror(errno));
                    ::close(m_ModuleFd);
                    m_ModuleFd = -1;
                    return false;
                }

                m_IsModuleLoaded = false;
                m_AllAllocatedBuffers.clear();

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

        ::close(m_ModuleFd);
        m_ModuleFd = -1;

        for (const auto &buffer_name : requested_buffers)
        {
            if (std::find(m_AllAllocatedBuffers.begin(), m_AllAllocatedBuffers.end(), buffer_name) == m_AllAllocatedBuffers.end())
            {
                m_AllAllocatedBuffers.push_back(buffer_name);
            }
        }

        m_IsModuleLoaded = true;
        SPDLOG_INFO("Successfully allocated {} DMA buffers", cfg.buffer_vec.size());
        return true;
    }

    bool UDmaBufferAllocate::DeallocateBuffers(const std::vector<std::string> &buffer_names)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        for (const auto &buffer_name : buffer_names)
        {
            auto it = std::find(m_AllAllocatedBuffers.begin(), m_AllAllocatedBuffers.end(), buffer_name);
            if (it != m_AllAllocatedBuffers.end())
            {
                m_AllAllocatedBuffers.erase(it);
            }
        }

        return true;
    }

    std::vector<std::string> UDmaBufferAllocate::GetAllocatedBuffers() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        std::filesystem::path class_path(U_DMA_BUF_CLASS_PATH);
        if (!std::filesystem::exists(class_path))
        {
            return {};
        }

        std::vector<std::string> buffer_list;
        try
        {
            for (const auto &entry : std::filesystem::directory_iterator(class_path))
            {
                if (entry.is_directory())
                {
                    std::string buffer_name = entry.path().filename().string();
                    if (buffer_name.find("udmabuf") == 0)
                    {
                        buffer_list.push_back(buffer_name);
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            SPDLOG_ERROR("Error reading u-dma-buf class directory: {}", e.what());
        }

        return buffer_list;
    }

    uint8_t UDmaBufferAllocate::GetAllocatedBuffersCount() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return static_cast<uint8_t>(m_AllAllocatedBuffers.size());
    }

    bool UDmaBufferAllocate::IsModuleLoaded() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_IsModuleLoaded;
    }

    std::string UDmaBufferAllocate::GetModulePath() const
    {
        struct utsname uts{};
        if (uname(&uts) != 0)
        {
            SPDLOG_ERROR("Failed to get system information: {}", strerror(errno));
            return "";
        }

        std::string module_path = "/lib/modules/" + std::string(uts.release) + "/updates/" + std::string(U_DMA_BUF_MODULE_NAME);
        if (std::filesystem::exists(module_path))
        {
            return module_path;
        }

        std::string alt_path = "/lib/modules/" + std::string(uts.release) + "/extra/" + std::string(U_DMA_BUF_MODULE_NAME);
        if (std::filesystem::exists(alt_path))
        {
            return alt_path;
        }

        SPDLOG_ERROR("Cannot find u-dma-buf module in standard locations");
        return "";
    }
}
