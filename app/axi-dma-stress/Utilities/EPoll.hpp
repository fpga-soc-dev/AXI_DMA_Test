#pragma once

#include <functional>
#include <map>

namespace Utils
{

    class EPoll
    {
    public:
        EPoll();
        ~EPoll();

        enum class TriggerOP
        {
            Read = 0x01,
            Write = 0x02,
            Error = 0x04
        };

        void Add(int fd, TriggerOP op, const std::function<void(int)> &callback);
        void Remove(int fd);
        void Poll();

    private:
        int _fd;
        std::map<int, std::function<void(int)>> _callbacks;
    };

} // namespace Embedded