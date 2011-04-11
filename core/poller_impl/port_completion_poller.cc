#include "pch.h"

#include "config.h"

#ifndef USE_PORT_COMPLETION
#error "port completion is not supported"
#endif

#include <port.h>
#include <poll.h>
#include <limits.h>

#include "utils/exception.h"
#include "utils/logger.h"
#include "utils/misc.h"
#include "core/poller.h"
#include "core/pipeline.h"

namespace tube {

#define OPS_EVENT 254

#define PENDING_OPS_ADD 0
#define PENDING_OPS_DEL 1

class PortPoller : public Poller
{
    int port_;

    struct PendingOps {
        int         ops;
        union {
            Connection* conn;
            int         fd;
        } ops_data;
    };
    utils::Mutex            mutex_;
    std::queue<PendingOps>  pending_;
public:
    PortPoller() ;
    virtual ~PortPoller();

    virtual void handle_event(int timeout) ;
    virtual bool poll_add_fd(int fd, Connection* conn, PollerEvent evt);
    virtual bool poll_remove_fd(int fd);
};

PortPoller::PortPoller() 
    : Poller()
{
    port_ = ::port_create();
    if (port_ < 0) {
        throw utils::SyscallException();
    }
}

PortPoller::~PortPoller()
{
    ::close(port_);
}

static int
build_port_event(PollerEvent evt)
{
    int res = 0;
    if (evt & POLLER_EVENT_READ) res |= POLLIN;
    if (evt & POLLER_EVENT_WRITE) res |= POLLOUT;
    if (evt & POLLER_EVENT_ERROR) res |= POLLERR;
    if (evt & POLLER_EVENT_HUP) res |= POLLHUP;
    return res;
}

static PollerEvent
build_poller_event(int events)
{
    PollerEvent evt = 0;
    if (events & POLLIN) evt |= POLLER_EVENT_READ;
    if (events & POLLOUT) evt |= POLLER_EVENT_WRITE;
    if (events & POLLERR) evt |= POLLER_EVENT_ERROR;
    if (events & POLLHUP) evt |= POLLER_EVENT_HUP;
    return evt;
}

bool
PortPoller::poll_add_fd(int fd, Connection* conn, PollerEvent evt)
{
    PendingOps ops;
    ops.ops = PENDING_OPS_ADD;
    ops.ops_data.conn = conn;
    conn->poller_spec.data_int = build_port_event(evt);
    utils::Lock lk(mutex_);
    pending_.push(ops);
    port_send(port_, OPS_EVENT, NULL);
    return true;
}

bool
PortPoller::poll_remove_fd(int fd)
{
    PendingOps ops;
    ops.ops = PENDING_OPS_DEL;
    ops.ops_data.fd = fd;
    utils::Lock lk(mutex_);
    pending_.push(ops);
    return true;
}

#define MAX_EVENT_PER_GET 1024

void
PortPoller::handle_event(int timeout) 
{
    port_event_t* port_evt = (port_event_t*)
        malloc(sizeof(port_event_t) * MAX_EVENT_PER_GET);
    timespec_t tspec;
    tspec.tv_sec = timeout;
    tspec.tv_nsec = 0;
    Connection* conn = NULL;
    while (true) {
        uint_t nfds = 0;
        /* weird handling.
         * 1. getn seems return immediately when no fd is associated
         * 2. get/getn will return error on timeout
         */
        if (::port_getn(port_, port_evt, MAX_EVENT_PER_GET, &nfds, &tspec)
            == -1) {
            if (errno == EINTR) {
                continue;
            } else if (errno == ETIME) {
                if (nfds == 0) {
                    continue;
                }
            } else {
                free(port_evt);
                throw utils::SyscallException();
            }
        }
        if (nfds == 0) {
            if (::port_get(port_, port_evt, &tspec) == -1) {
                if (errno == EINTR) {
                    continue;
                } else if (errno == ETIME) {
                    continue;
                } else {
                    free(port_evt);
                    throw utils::SyscallException();
                }
            }
            nfds = 1;
        }
        if (!pre_handler_.empty())
            pre_handler_(*this);
        if (!handler_.empty()) {
            for (uint_t i = 0; i < nfds; i++) {
                if (port_evt[i].portev_source != PORT_SOURCE_FD) {
                    continue;
                }
                conn = (Connection*) port_evt[i].portev_user;
                handler_(conn, build_poller_event(port_evt[i].portev_events));
            }
        }
        if (!post_handler_.empty()) {
            post_handler_(*this);
        }
        // reassociate all the fds
        for (uint_t i = 0; i < nfds; i++) {
            if (port_evt[i].portev_source != PORT_SOURCE_FD) {
                continue;
            }
            conn = (Connection*) port_evt[i].portev_user;
            ::port_associate(port_, PORT_SOURCE_FD, conn->fd,
                             conn->poller_spec.data_int, conn);
        }
        // applying the associate/disassociate operations
        utils::Lock lk(mutex_);
        while (!pending_.empty()) {
            PendingOps& ops = pending_.front();
            if (ops.ops == PENDING_OPS_ADD) {
                Connection* conn = ops.ops_data.conn;
                ::port_associate(port_, PORT_SOURCE_FD, conn->fd,
                                 conn->poller_spec.data_int, conn);
            } else if (ops.ops == PENDING_OPS_DEL) {
                ::port_dissociate(port_, PORT_SOURCE_FD, ops.ops_data.fd);
            }
            pending_.pop();
        }
    }
}

EXPORT_POLLER_IMPL(port_completion, PortPoller);

}
