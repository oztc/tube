#include <limits.h>
#include <sys/epoll.h>

#include "utils/exception.h"
#include "core/poller.h"

namespace pipeserv {

class EpollPoller : public Poller
{
    int epoll_fd_;
public:
    EpollPoller() throw();
    virtual ~EpollPoller();

    virtual void handle_event(int timeout) throw();
    virtual bool add_fd(int fd, Connection* conn, PollerEvent evt);
    virtual bool remove_fd(int fd);
};

EpollPoller::EpollPoller() throw()
    : Poller()
{
    epoll_fd_ = epoll_create(INT_MAX); // should ignore
    if (epoll_fd_ < 0) {
        throw utils::SyscallException();
    }
}

EpollPoller::~EpollPoller()
{
    close(epoll_fd_);
}

static int
build_epoll_event(PollerEvent evt)
{
    int res = 0;
    if (evt & POLLER_EVENT_READ) res |= EPOLLIN;
    if (evt & POLLER_EVENT_WRITE) res |= EPOLLOUT;
    if (evt & POLLER_EVENT_ERROR) res |= EPOLLERR;
    if (evt & POLLER_EVENT_HUP) res |= EPOLLHUP;
    return res;
}

static PollerEvent
build_poller_event(int events)
{
    PollerEvent evt;
    if (events & EPOLLIN) evt |= POLLER_EVENT_READ;
    if (events & EPOLLOUT) evt |= POLLER_EVENT_WRITE;
    if (events & EPOLLERR) evt |= POLLER_EVENT_ERROR;
    if (events & EPOLLHUP) evt |= POLLER_EVENT_HUP;
    return evt;
}

bool
EpollPoller::add_fd(int fd, Connection* conn, PollerEvent evt)
{
    if (add_fd_set(fd)) {
        struct epoll_event epoll_evt;
        epoll_evt.events = build_epoll_event(evt);
        epoll_evt.data.ptr = conn;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &epoll_evt) < 0) {
            remove_fd_set(fd);
            return false;
        }
        return true;
    }
    return false;
}

bool
EpollPoller::remove_fd(int fd)
{
    if (remove_fd_set(fd)) {
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, NULL) < 0) {
            add_fd_set(fd);
            return false;
        }
        return true;
    }
    return false;
}

#define MAX_EVENT_PER_POLL 4096

void
EpollPoller::handle_event(int timeout) throw()
{
    struct epoll_event* epoll_evt = (struct epoll_event*)
        malloc(sizeof(struct epoll_event) * MAX_EVENT_PER_POLL);
    Connection* conn = NULL;
    while (true) {
        int nfds = epoll_wait(epoll_fd_, epoll_evt, MAX_EVENT_PER_POLL,
                              timeout);
        if (nfds < 0) {
            free(epoll_evt);
            throw utils::SyscallException();
        }
        for (int i = 0; i < nfds; i++) {
            conn = (Connection*) epoll_evt[i].data.ptr;
            handler_(conn, build_poller_event(epoll_evt[i].events));
        }
    }
    free(epoll_evt);
}

struct EpollPollerRegister
{
    EpollPollerRegister() {
        PollerFactory::instance().register_poller(
            "epoll", boost::bind(&EpollPollerRegister::create_poller, this));
    }

    Poller* create_poller() const {
        return new EpollPoller();
    }
};

static EpollPollerRegister __epoll_poller_register;

}
