#include <cassert>
#include <cstdlib>
#include <boost/bind.hpp>

#include "utils/exception.h"
#include "utils/logger.h"
#include "core/stages.h"
#include "core/pipeline.h"

using namespace pipeserv::utils;

namespace pipeserv {

Stage::Stage(std::string name)
{
    LOG(INFO, "adding %s stage to pipeline", name.c_str());
    Pipeline::instance().add_stage(name, this);
}

bool
Stage::sched_add(Connection* conn)
{
    if (sched_) {
        sched_->add_task(conn);
    }
    return true;
}

void
Stage::sched_remove(Connection* conn)
{
    if (sched_) {
        sched_->remove_task(conn);
    }
}

void
Stage::main_loop()
{
    if (!sched_) return;
    while (true) {
        Connection* conn = sched_->pick_task();
        process_task(conn);
        conn->unlock();
    }
}

void
Stage::start_thread()
{
    Thread th(boost::bind(&Stage::main_loop, this));
}

PollInStage::PollInStage(int max_event)
    : Stage("poll_in")
{
    sched_ = NULL; // no scheduler, need to override ``sched_add``
    max_event_ = max_event;
    timeout_ = -1; // no timeout by default
    tmp_buf_size_ = 4096; // 4k for tmp buf

    epoll_fd_ = epoll_create(max_event_);
    if (epoll_fd_ < 0)
        throw SyscallException();
}

PollInStage::~PollInStage()
{
    close(epoll_fd_);
}

void
PollInStage::initialize()
{
    parser_stage_ = Pipeline::instance().find_stage("parser");
    if (parser_stage_ == NULL)
        throw std::invalid_argument("cannot find parser stage");
    recycle_stage_ = Pipeline::instance().find_stage("recycle");
    if (recycle_stage_ == NULL)
        throw std::invalid_argument("cannot find recycle stage");
}

bool
PollInStage::sched_add(Connection* conn)
{
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    ev.data.ptr = conn;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, conn->fd, &ev) < 0) {
        return false;
    }
    return true;
}

void
PollInStage::sched_remove(Connection* conn)
{
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, conn->fd, NULL);
}

void
PollInStage::cleanup_connection(int client_fd, Connection* conn)
{
    assert(conn);
    shutdown(conn->fd, SHUT_RDWR);
    conn->inactive = true;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, conn->fd, NULL);
    recycle_stage_->sched_add(conn);
}

void
PollInStage::read_connection(int client_fd, Connection* conn)
{
    assert(conn);

    ssize_t sz = -1;
    byte buf[tmp_buf_size_];

    if (!conn->trylock()) // avoid lock contention
        return;

    while (true) {
        sz = read(client_fd, buf, tmp_buf_size_);
        if (sz <= 0) {
            conn->unlock();
            if (sz == 0 || !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                // according to man pages, 0 means end of file
                cleanup_connection(client_fd, conn);
            } else {
                parser_stage_->sched_add(conn);
            }
            return;
        } else {
            // append read data into input buffer
            conn->in_buf.append(buf, sz);
        }
    }
}

void
PollInStage::main_loop()
{
    struct epoll_event ev[max_event_];
    while (true) {
        int nfds = epoll_wait(epoll_fd_, ev, max_event_, timeout_);
        for (int i = 0; i < nfds; i++) {
            Connection* conn = (Connection*) ev[i].data.ptr;
            int client_fd = conn->fd;
            if ((ev[i].events & EPOLLHUP) || (ev[i].events & EPOLLERR)) {
                // error happened, close the fd
                cleanup_connection(client_fd, conn);
            } else if (ev[i].events & EPOLLIN) {
                read_connection(client_fd, conn);
            }
        }
    }
}

PollOutStage::PollOutStage(int max_event)
    : Stage("poll_out")
{
    sched_ = NULL;
    max_event_ = max_event;
    timeout_ = -1;

    epoll_fd_ = epoll_create(max_event_);
    if (epoll_fd_ < 0)
        throw SyscallException();
}

PollOutStage::~PollOutStage()
{
    close(epoll_fd_);
}

void
PollOutStage::initialize()
{
}

bool
PollOutStage::sched_add(Connection* conn)
{
    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
    ev.data.ptr = conn;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, conn->fd, &ev) < 0) {
        return false;
    }
    return true;
}

void
PollOutStage::sched_remove(Connection* conn)
{
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, conn->fd, NULL) == 0) {
        conn->hold = false; // give up the lock
        conn->unlock();
    }
}

void
PollOutStage::main_loop()
{
    struct epoll_event ev[max_event_];
    while (true) {
        int nfds = epoll_wait(epoll_fd_, ev, max_event_, timeout_);
        for (int i = 0; i < nfds; i++) {
            Connection* conn = (Connection*) ev[i].data.ptr;
            if (ev[i].events & EPOLLOUT) {
                ssize_t sz = write(conn->fd, conn->out_buf.ptr(),
                                   conn->out_buf.size());
                if (sz > 0) {
                    conn->out_buf.pop(sz);
                    continue;
                }
            }
            // error happened
            sched_remove(conn);
        }
    }
}

ParserStage::ParserStage()
    : Stage("parser")
{
    sched_ = new QueueScheduler();
}

ParserStage::~ParserStage()
{
    delete sched_;
}

bool
RecycleStage::sched_add(Connection* conn)
{
    utils::Lock lk(mutex_);
    queue_.push(conn);
    if (queue_.size() >= recycle_batch_size_) {
        cond_.notify_one();
    }
    return true;
}

void
RecycleStage::main_loop()
{
    Pipeline& pipeline = Pipeline::instance();
    Connection* conns[recycle_batch_size_];
    while (true) {
        mutex_.lock();
        while (queue_.size() < recycle_batch_size_) {
            cond_.wait(mutex_);
        }
        for (int i = 0; i < recycle_batch_size_; i++) {
            conns[i] = queue_.front();
            queue_.pop();
        }
        mutex_.unlock();

        utils::XLock lk(pipeline.mutex());
        for (int i = 0; i < recycle_batch_size_; i++) {
            pipeline.dispose_connection(conns[i]);
        }
    }
}

}
