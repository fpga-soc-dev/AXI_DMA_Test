#include "EPoll.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>
#include <vector>

namespace Utils
{

    EPoll::EPoll() : _fd(epoll_create1(0))
    {
        if (_fd == -1)
        {
            throw std::runtime_error("Failed to create epoll file descriptor");
        }
    }

    EPoll::~EPoll()
    {
        close(_fd);
    }

    void EPoll::Add(int fd, TriggerOP op, const std::function<void(int)> &callback)
    {
        if (fd < 0)
        {
            throw std::invalid_argument("Invalid file descriptor");
        }

        epoll_event event;
        event.events = 0;
        event.data.fd = fd;
        if (static_cast<int>(op) & static_cast<int>(TriggerOP::Read))
        {
            event.events |= EPOLLIN;
        }
        else if (static_cast<int>(op) & static_cast<int>(TriggerOP::Write))
        {
            event.events |= EPOLLOUT;
        }
        else if (static_cast<int>(op) & static_cast<int>(TriggerOP::Error))
        {
            event.events |= EPOLLERR;
        }
        else
        {
            throw std::runtime_error("Invalid TriggerOP");
        }

        if (epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) == -1)
        {
            throw std::runtime_error("Failed to add file descriptor to epoll");
        }
        _callbacks[fd] = callback;
    }

    void EPoll::Remove(int fd)
    {
        if (fd < 0)
        {
            throw std::invalid_argument("Invalid file descriptor");
        }

        if (epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) == -1)
        {
            throw std::runtime_error("Failed to remove file descriptor from epoll");
        }
        _callbacks.erase(fd);
    }

    void EPoll::Poll()
    {
        if (_callbacks.empty())
        {
            return;
        }

        std::vector<epoll_event> events{_callbacks.size(), epoll_event{0}};
        auto event_count{epoll_wait(_fd, events.data(), events.size(), -1)};
        if (event_count == -1 && errno != EINTR)
        {
            throw std::runtime_error("Failed to poll file descriptors");
        }

        for (auto i = 0; i < event_count; ++i)
        {
            _callbacks[events[i].data.fd](events[i].data.fd);
        }
    }

} // namespace Embedded