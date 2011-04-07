#include "pch.h"

#include "config.h"
#include "core/poller.h"

namespace tube {

Poller::Poller() throw()
{
}

bool
Poller::has_fd(int fd) const
{
    return (fds_.find(fd) != fds_.end());
}

Connection*
Poller::find_connection(int fd)
{
    FDMap::iterator it = fds_.find(fd);
    if (it == fds_.end()) {
        return NULL;
    } else {
        return *it;
    }
}

bool
Poller::add_fd_set(int fd, Connection* conn)
{
    return fds_.insert(fd, conn);
}

bool
Poller::remove_fd_set(int fd)
{
    return fds_.erase(fd);
}

bool
Poller::add_fd(int fd, Connection* conn, PollerEvent evt)
{
    if (add_fd_set(fd, conn)) {
        if (!poll_add_fd(fd, conn, evt)) {
            remove_fd_set(fd);
            goto failed;
        }
        return true;
    }
failed:
    return false;
}

bool
Poller::remove_fd(int fd)
{
    Connection* conn = find_connection(fd);
    if (conn) {
        remove_fd_set(fd);
        if (!poll_remove_fd(fd)) {
            add_fd_set(fd, conn);
            goto failed;
        }
        return true;
    }
failed:
    return false;
}

void
PollerFactory::register_poller(const char* name, CreateFunc create_func)
{
    poller_map_.insert(std::make_pair(std::string(name), create_func));
}

Poller*
PollerFactory::create_poller(const std::string& name)
{
    PollerMap::iterator it = poller_map_.find(name);
    if (it == poller_map_.end() || !(it->second)) {
        return NULL;
    }
    return it->second();
}

void
PollerFactory::destroy_poller(Poller* poller)
{
    delete poller;
}

std::string
PollerFactory::default_poller_name() const
{
#ifdef USE_EPOLL
    return std::string("epoll");
#endif
#ifdef USE_KQUEUE
    return std::string("kqueue");
#endif
#ifdef USE_PORT_COMPLETION
    return std::string("port_completion");
#endif
}

}
