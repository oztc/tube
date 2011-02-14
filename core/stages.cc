#include <cassert>
#include <cstdlib>
#include <limits.h>
#include <boost/bind.hpp>

#include "utils/exception.h"
#include "utils/logger.h"
#include "utils/misc.h"
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
        if (process_task(conn) >= 0)
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
    timeout_ = 10; // 10s by default
}

PollInStage::~PollInStage()
{
    for (int i = 0; i < polls_.size(); i++) {
        close(polls_[i].poll_fd);
    }
}

void
PollInStage::add_poll(int poll_fd)
{
    utils::Lock lk(mutex_);
    polls_.push_back(Poll(poll_fd));
}

bool
PollInStage::sched_add(Connection* conn)
{
    utils::Lock lk(mutex_);
    // pick the poll with minimum size
    int min_size = INT_MAX;
    int poll_idx = -1;
    for (int i = 0; i < polls_.size(); i++) {
        size_t nfds = polls_[i].fds.size();
        if (min_size > nfds) {
            poll_idx = i;
            min_size = nfds;
        }
    }
    if (poll_idx < 0) {
        return false;
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = conn;
    polls_[poll_idx].fds.insert(conn->fd);
    if (epoll_ctl(polls_[poll_idx].poll_fd, EPOLL_CTL_ADD, conn->fd, &ev) < 0) {
        return false;
    }
    return true;
}

void
PollInStage::sched_remove(Connection* conn)
{
    utils::Lock lk(mutex_);
    for (int i = 0; i < polls_.size(); i++) {
        PollSet& pollset = polls_[i].fds;
        PollSet::iterator it = pollset.find(conn->fd);
        if (it != pollset.end()) {
            pollset.erase(conn->fd);
            epoll_ctl(polls_[i].poll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
            return;
        }
    }
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

void
PollInStage::cleanup_connection(Connection* conn)
{
    assert(conn);

    //fprintf(stderr, "%d cleanup %d\n", utils::get_thread_id(), conn->fd);
    shutdown(conn->fd, SHUT_RDWR);
    conn->inactive = true;
    sched_remove(conn);
    recycle_stage_->sched_add(conn);
}

void
PollInStage::read_connection(Connection* conn)
{
    assert(conn);

    if (!conn->trylock()) // avoid lock contention
        return;
    int nread = conn->in_buf.read_from_fd(conn->fd);
    conn->unlock();

    if (nread < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        parser_stage_->sched_add(conn);
    } else {
        cleanup_connection(conn);
    }
}

void
PollInStage::main_loop()
{
    int poll_fd = epoll_create(max_event_);
    if (poll_fd < 0) {
        throw SyscallException();
        return;
    }
    struct epoll_event ev[max_event_];
    add_poll(poll_fd);
    while (true) {
        int nfds = epoll_wait(poll_fd, ev, max_event_, timeout_);
        for (int i = 0; i < nfds; i++) {
            Connection* conn = (Connection*) ev[i].data.ptr;
            if ((ev[i].events & EPOLLHUP) || (ev[i].events & EPOLLERR)) {
                cleanup_connection(conn);
            } else if (ev[i].events & EPOLLIN) {
                read_connection(conn);
            }
        }
    }
}

WriteBackStage::WriteBackStage()
    : Stage("write_back")
{
    sched_ = new QueueScheduler(true);
}

WriteBackStage::~WriteBackStage()
{
    delete sched_;
}

int
WriteBackStage::process_task(Connection* conn)
{
    utils::set_socket_blocking(conn->fd, true);
    Buffer& buf = conn->out_buf;
    int rs = buf.write_to_fd(conn->fd);
    utils::set_socket_blocking(conn->fd, false);

    if (buf.size() > 0 && rs > 0) {
        sched_add(conn);
        return -1;
    } else {
        return 0; // done, and not to schedule it anymore
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
