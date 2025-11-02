#pragma once

#include <fstream>
#include <stdexcept>
#include <sstream>
#include <unistd.h>
#include <string>
#include <cstring>

namespace Utils
{
    template <typename T>
    static T ReadFromFile(const std::string &file_name)
    {
        std::ifstream file(file_name);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + file_name);
        }

        T value;
        if (!(file >> value))
        {
            throw std::runtime_error("Failed to read value from file: " + file_name);
        }

        return value;
    }

    template <typename T, std::ios_base::fmtflags Fmt>
    static T ReadFromFile(const std::string &file_name)
    {
        std::ifstream file(file_name);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + file_name);
        }

        T value;
        file.setf(Fmt, std::ios_base::basefield);
        file >> value;
        return value;
    }

    static std::string BuildInstanceName(uint8_t instance_index)
    {
        std::ostringstream oss;
        oss << "uio" << static_cast<int>(instance_index);
        return oss.str();
    }

    class Pipe
    {
    public:
        Pipe()
        {
            if (pipe(pipe_fds) == -1)
            {
                throw std::runtime_error("Failed to create pipe: " + std::string(strerror(errno)));
            }
        }

        ~Pipe()
        {
            close(pipe_fds[0]);
            close(pipe_fds[1]);
        }

        void Write(const void *data, std::size_t size) const
        {
            ssize_t result = write(pipe_fds[1], data, size);
            if (result == -1)
            {
                throw std::runtime_error("Failed to write to pipe: " + std::string(strerror(errno)));
            }
        }

        std::size_t Read(void *buffer, std::size_t size) const
        {
            ssize_t result = read(pipe_fds[0], buffer, size);
            if (result == -1)
            {
                throw std::runtime_error("Failed to read from pipe: " + std::string(strerror(errno)));
            }
            return static_cast<std::size_t>(result);
        }

        int ReadHandle() const { return pipe_fds[0]; }
        int WriteHandle() const { return pipe_fds[1]; }

    private:
        int pipe_fds[2];
    };
} // namespace Utils