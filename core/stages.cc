#include "pch.h"

#include <cassert>
#include <cstdlib>
#include <limits.h>

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
    sched_ = NULL;
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
Stage::sched_reschedule()
{
    if (sched_) {
        sched_->reschedule();
    }
}

void
Stage::main_loop()
{
    if (!sched_) return;
    while (true) {
        Connection* conn = sched_->pick_task();
        if (process_task(conn) >= 0) {
            conn->unlock();
            pipeline_.reschedule_all();
        }
    }
}

void
Stage::start_thread()
{
    Thread th(boost::bind(&Stage::main_loop, this));
}

IdleScanner::IdleScanner(int scan_timeout, PollInStage& stage)
    : scan_timeout_(scan_timeout), stage_(stage)
{
    last_scan_time_ = time(NULL);
}

void
IdleScanner::scan_idle_connection(Poller& poller)
{
    if (!stage_.mutex_.try_lock()) {
        return;
    }

    uint32_t current_time = time(NULL);
    int idled_time = current_time - last_scan_time_;
    if (idled_time < scan_timeout_) {
        return;
    }
    std::vector<Connection*> timeout_connections;
    for (Poller::FDMap::iterator it = poller.begin(); it != poller.end();
         ++it) {
        Connection* conn =  it->second;
        // on most architecture, accessing two words should be atomic.
        if (conn->timeout <= 0)
            continue;
        if (current_time - conn->last_active > conn->timeout) {
            timeout_connections.push_back(conn);
        }
    }
    for (size_t i = 0; i < timeout_connections.size(); i++) {
         // timeout: this connection has been idle for a long time.
        Connection* conn = timeout_connections[i];
        LOG(INFO, "connection %d has timeout", conn->fd);
        stage_.cleanup_connection(conn);
    }

    last_scan_time_ = current_time;
    stage_.mutex_.unlock();
}

int PollInStage::kDefaultTimeout = 10;

PollInStage::PollInStage()
    : Stage("poll_in")
{
    sched_ = NULL; // no scheduler, need to override ``sched_add``
    timeout_ = kDefaultTimeout; // 10s by default
    poller_name_ = PollerFactory::instance().default_poller_name();
    current_poller_ = 0;
}

PollInStage::~PollInStage()
{
    for (size_t i = 0; i < pollers_.size(); i++) {
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
    current_poller_ = (current_poller_ + 1) % pollers_.size();
    return pollers_[current_poller_]->add_fd(
        conn->fd, conn, POLLER_EVENT_READ | POLLER_EVENT_ERROR
        | POLLER_EVENT_HUP);
}

void
PollInStage::sched_remove(Connection* conn)
{
    utils::Lock lk(mutex_);
    for (size_t i = 0; i < pollers_.size(); i++) {
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
        nread = conn->in_stream.read_into_buffer();
    } while (nread > 0);
    conn->last_active = time(NULL);
    conn->unlock();
    pipeline_.reschedule_all();

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
    IdleScanner idle_scanner(timeout_, *this);
    Poller::EventCallback evthdl =
        boost::bind(&PollInStage::handle_connection, this, _1, _2);
    Poller::PollerCallback posthdl =
        boost::bind(&IdleScanner::scan_idle_connection, &idle_scanner, _1);

    poller->set_post_handler(posthdl);
    poller->set_event_handler(evthdl);
    add_poll(poller);
    poller->handle_event(timeout_);
    delete poller;
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
    conn->last_active = time(NULL);
    utils::set_socket_blocking(conn->fd, true);
    OutputStream& out = conn->out_stream;
    int rs = out.write_into_output();
    utils::set_socket_blocking(conn->fd, false);

    if (!out.is_done() && rs > 0) {
        conn->last_active = time(NULL);
        sched_add(conn);
        return -1;
    } else {
        if (conn->close_after_finish) {
            conn->active_close();
        }
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
        for (size_t i = 0; i < recycle_batch_size_; i++) {
            conns[i] = queue_.front();
            queue_.pop();
        }
        mutex_.unlock();

        utils::XLock lk(pipeline.mutex());
        for (size_t i = 0; i < recycle_batch_size_; i++) {
            pipeline.dispose_connection(conns[i]);
        }
    }
}

}
