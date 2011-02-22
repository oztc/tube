#include <cassert>
#include <cstdlib>
#include <limits.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>

#include "utils/exception.h"
#include "utils/logger.h"
#include "utils/misc.h"
#include "core/stages.h"
#include "core/pipeline.h"

using namespace pipeserv::utils;

namespace pipeserv {

Stage::Stage(std::string name)
    : pipeline_(Pipeline::instance())
{
    LOG(INFO, "adding %s stage to pipeline", name.c_str());
    pipeline_.add_stage(name, this);
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
    timeout_ = 10; // 10s by default
    poller_name_ = PollerFactory::instance().default_poller_name();
}

PollInStage::~PollInStage()
{
    for (int i = 0; i < pollers_.size(); i++) {
        PollerFactory::instance().destroy_poller(pollers_[i]);
    }
}

void
PollInStage::add_poll(Poller* poller)
{
    utils::Lock lk(mutex_);
    pollers_.push_back(poller);
}

bool
PollInStage::sched_add(Connection* conn)
{
    utils::Lock lk(mutex_);
    // pick the poll with minimum size
    int min_size = INT_MAX;
    int poll_idx = -1;
    for (int i = 0; i < pollers_.size(); i++) {
        size_t nfds = pollers_[i]->size();
        if (min_size > nfds) {
            poll_idx = i;
            min_size = nfds;
        }
    }
    if (poll_idx < 0) {
        return false;
    }
    return pollers_[poll_idx]->add_fd(conn->fd, conn, POLLER_EVENT_READ |
                                      POLLER_EVENT_ERROR | POLLER_EVENT_HUP);
}

void
PollInStage::sched_remove(Connection* conn)
{
    utils::Lock lk(mutex_);
    for (int i = 0; i < pollers_.size(); i++) {
        if (pollers_[i]->remove_fd(conn->fd))
            return;
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
    int nread;
    do {
        nread = conn->in_buf.read_from_fd(conn->fd);
    } while (nread > 0);
    conn->unlock();

    if (nread < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        parser_stage_->sched_add(conn);
    } else {
        cleanup_connection(conn);
    }
}

void
PollInStage::handle_connection(Connection* conn, PollerEvent evt)
{
    if ((evt & POLLER_EVENT_HUP) || (evt & POLLER_EVENT_ERROR)) {
        cleanup_connection(conn);
    } else if (evt & POLLER_EVENT_READ) {
        read_connection(conn);
    }
}

void
PollInStage::main_loop()
{
    Poller* poller = PollerFactory::instance().create_poller(poller_name_);
    Poller::PollerCallback func = boost::bind(&PollInStage::handle_connection,
                                              this, _1, _2);
    poller->set_handler(func);
    add_poll(poller);
    poller->handle_event(timeout_);
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
