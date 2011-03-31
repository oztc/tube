#include "pch.h"

#include "config.h"
#include "core/poller.h"

namespace pipeserv {

Poller::Poller() throw()
{
    fds_.set_empty_key(-INT_MAX);
    fds_.set_deleted_key(INT_MAX);
}

bool
Poller::has_fd(int fd) const
{
    if (fds_.find(fd) == fds_.end())
        return false;
    return true;
}

Connection*
Poller::find_connection(int fd)
{
    FDMap::iterator it = fds_.find(fd);
    if (it == fds_.end())
        return NULL;
    return it->second;
}

bool
Poller::add_fd_set(int fd, Connection* conn)
{
    return fds_.insert(std::make_pair(fd, conn)).second;
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
#else
#ifdef USE_KQUEUE
    return std::string("kqueue");
#else
#error "Not ported to this platform"
#endif
#endif
}

}
