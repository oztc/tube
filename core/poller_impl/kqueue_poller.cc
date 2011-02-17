#include "config.h"

#ifndef USE_KQUEUE
#error "kqueue is not supported"
#endif

#include <limits.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include "utils/exception.h"
#include "core/poller.h"

namespace pipeserv {

class KqueuePoller : public Poller
{
    int kqueue_;
public:
    KqueuePoller() throw();
    virtual ~KqueuePoller();

    virtual void handle_event(int timeout) throw();
    virtual bool add_fd(int fd, Connection* conn, PollerEvent evt);
    virtual bool remove_fd(int fd);
};

KqueuePoller::KqueuePoller() throw()
{
    kqueue_ = kqueue();
    if (kqueue_ < 0) {
        throw utils::SyscallException();
    }
}

KqueuePoller::~KqueuePoller()
{
    close(kqueue_);
}

static int
build_kqueue_filter(PollerEvent evt)
{
    int res = 0;
    if (evt & POLLER_EVENT_READ) res |= EVFILT_READ;
    if (evt & POLLER_EVENT_WRITE) res |= EVFILT_WRITE;
    return res;
}

static PollerEvent
build_poller_event(short filter, u_short events)
{
    PollerEvent evt;
    if (filter & EVFILT_READ) evt |= POLLER_EVENT_READ;
    if (filter & EVFILT_WRITE) evt |= POLLER_EVENT_WRITE;
    if (events & EV_EOF) evt |= POLLER_EVENT_HUP;
    if (events & EV_ERROR) evt |= POLLER_EVENT_ERROR;
    return evt;
}

bool
KqueuePoller::add_fd(int fd, Connection* conn, PollerEvent evt)
{
    if (add_fd_set(fd)) {
        struct kevent kqueue_event;
        struct timespec zero;
        EV_SET(&kqueue_event, fd, build_kqueue_filter(evt), EV_ADD | EV_ENABLE,
               0, 0, conn);
        memset(&zero, 0, sizeof(timespec));
        if (kevent(kqueue_, &kqueue_event, 1, NULL, 0, &zero) < 0) {
            remove_fd_set(fd);
            goto failed;
        }
        return true;
    }
failed:
    return false;
}

bool
KqueuePoller::remove_fd(int fd)
{
    if (remove_fd_set(fd)) {
        struct kevent kqueue_event;
        struct timespec zero;
        EV_SET(&kqueue_event, fd, EVFILT_READ |  EVFILT_WRITE ,
               EV_DELETE | EV_DISABLE, 0, 0, NULL);
        memset(&zero, 0, sizeof(timespec));
        if (kevent(kqueue_, &kqueue_event, 1, NULL, 0, &zero) < 0) {
            if (errno != ENOENT) {
                add_fd_set(fd);
            }
            goto failed;
        }
        return true;
    }
failed:
    return false;
}

#define MAX_EVENT_PER_KEVENT 4096

void
KqueuePoller::handle_event(int timeout) throw ()
{
    struct kevent* kevents = (struct kevent*)
        malloc(sizeof(struct kevent) * MAX_EVENT_PER_KEVENT);
    Connection* conn = NULL;
    struct timespec tspec;
    tspec.tv_sec = timeout;
    tspec.tv_nsec = 0;
    while (true) {
        int nfds = ::kevent(kqueue_, NULL, 0, kevents, MAX_EVENT_PER_KEVENT,
p                            &tspec);
        if (nfds < 0) {
            free(kevents);
            throw utils::SyscallException();
        }
        for (int i = 0; i < nfds; i++) {
            conn = (Connection*) kevents[i].udata;
            handler_(conn, build_poller_event(kevents[i].filter,
                                              kevents[i].flags));
        }
    }
    free(kevents);
}

struct KqueuePollerRegister
{
    KqueuePollerRegister() {
        PollerFactory::instance().register_poller(
            "kqueue", boost::bind(&KqueuePollerRegister::create_poller, this));
    }

    Poller* create_poller() const {
        return new KqueuePoller();
    }
};

static KqueuePollerRegister __kqueue_poller_register;

}
