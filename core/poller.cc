#include "core/poller.h"

namespace pipeserv {

Poller::Poller() throw()
{
    fds_.set_empty_key(-INT_MAX);
    fds_.set_deleted_key(INT_MAX);
}

void
PollerFactory::register_poller(const char* name, CreateFunc create_func)
{
    poller_map_.insert(std::make_pair(std::string(name), create_func));
}

Poller*
PollerFactory::create_poller(std::string name)
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
#ifdef __linux__
    return std::string("epoll");
#else
#error "Not ported to this platform"
#endif
}

}
