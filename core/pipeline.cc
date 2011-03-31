#include "pch.h"

#include <ctime>
#include <unistd.h>
#include <netinet/tcp.h>

#include "core/pipeline.h"
#include "core/stages.h"
#include "utils/logger.h"
#include "utils/misc.h"

namespace pipeserv {

Connection::Connection(int sock)
    : in_stream(sock), out_stream(sock), close_after_finish(false)
{
    fd = sock;
    timeout = 0; // default no timeout
    prio = 0;
    inactive = false;
    last_active = time(NULL);

    // set nodelay
    int state = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &state, sizeof(state));
}

void
Connection::active_close()
{
    PollInStage* stage = (PollInStage*) Pipeline::instance().poll_in_stage();
    stage->cleanup_connection(this);
}

void
Connection::set_cork()
{
    int state = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state)) < 0) {
        LOG(WARNING, "Cannot set TCP_CORK on fd %d", fd);
    }
}

void
Connection::clear_cork()
{
    int state = 0;
    if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state)) < 0) {
        LOG(WARNING, "Cannot clear TCP_CORK on fd %d", fd);
    }
    ::fsync(fd);
}

void
Connection::set_io_timeout(int msec)
{
    struct timeval tm;
    tm.tv_sec = msec / 1000;
    tm.tv_usec = (msec % 1000) * 1000;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tm, sizeof(struct timeval))
        < 0) {
        LOG(WARNING, "Cannot set IO timeout on fd %d", fd);
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tm, sizeof(struct timeval))
        < 0) {
        LOG(WARNING, "Cannot set IO timeout on fd %d", fd);
    }
}

bool
Connection::trylock()
{
    if (mutex.try_lock()) {
#ifdef TRACK_OWNER
        owner = utils::get_thread_id();
#endif
        return true;
    }
    return false;
}

void
Connection::lock()
{
    mutex.lock();
#ifdef TRACK_OWNER
    owner = utils::get_thread_id();
#endif
}

void
Connection::unlock()
{
#ifdef TRACK_OWNER
    owner = -1;
#endif
    mutex.unlock();
}

std::string
Connection::address_string() const
{
    return address.address_string();
}

Scheduler::Scheduler()
{

}

Scheduler::~Scheduler()
{
}

QueueScheduler::QueueScheduler(bool suppress_connection_lock)
    : Scheduler(), suppress_connection_lock_(suppress_connection_lock)
{
    nodes_.set_empty_key(NULL);
    nodes_.set_deleted_key((Connection*) 0x08);
}

void
QueueScheduler::add_task(Connection* conn)
{
    utils::Lock lk(mutex_);
    NodeMap::iterator it = nodes_.find(conn);
    if (it != nodes_.end()) {
        // already in the scheduler, put it on the top
        NodeList::iterator node = it->second;
        list_.erase(node);
        list_.push_front(conn);
        nodes_.erase(it);
        nodes_.insert(std::make_pair(conn, list_.begin()));
        return;
    }
    bool need_notify = (list_.size() == 0);
    list_.push_back(conn);
    NodeList::iterator node = list_.end();
    nodes_.insert(std::make_pair(conn, --node));

    lk.unlock();
    if (need_notify) {
        cond_.notify_all();
    }
}

void
QueueScheduler::reschedule()
{
    if (!suppress_connection_lock_) {
        utils::Lock lk(mutex_);
        cond_.notify_all();
    }
}

class QueueSchedulerPickScope
{
    utils::Mutex&   mutex_;
    utils::RWMutex& pipemutex_;
public:
    QueueSchedulerPickScope(utils::Mutex& mutex)
        : mutex_(mutex), pipemutex_(Pipeline::instance().mutex()) {
        lock();
    }
    ~QueueSchedulerPickScope() { unlock(); }

    void lock() {
        pipemutex_.lock_shared();
        mutex_.lock();
    }

    void unlock() {
        mutex_.unlock();
        pipemutex_.unlock_shared();
    }

};

Connection*
QueueScheduler::pick_task_nolock_connection()
{
    utils::Lock lk(mutex_);
    while (list_.empty()) {
        cond_.wait(lk);
    }
    Connection* conn = list_.front();
    list_.pop_front();
    nodes_.erase(conn);
    return conn;
}

Connection*
QueueScheduler::pick_task_lock_connection()
{
    QueueSchedulerPickScope lk(mutex_);
    while (list_.empty()) {
        cond_.wait(lk);
    }

reschedule:
    Connection* conn = NULL;
    for (NodeList::iterator it = list_.begin(); it != list_.end(); ++it) {
        conn = *it;
        if (conn->trylock()) {
            list_.erase(it);
            nodes_.erase(conn);
            return conn;
        }
    }
    cond_.wait(lk);
    goto reschedule;
}

Connection*
QueueScheduler::pick_task()
{
    if (suppress_connection_lock_) {
        return pick_task_nolock_connection();
    } else {
        return pick_task_lock_connection();
    }
}

void
QueueScheduler::remove_task(Connection* conn)
{
    utils::Lock lk(mutex_);
    NodeMap::iterator it = nodes_.find(conn);
    if (it == nodes_.end()) {
        return;
    }
    NodeList::iterator node = it->second;
    list_.erase(node);
    nodes_.erase(it);
}

QueueScheduler::~QueueScheduler()
{
    utils::Lock lk(mutex_);
    list_.clear();
    nodes_.clear();
}

Connection*
ConnectionFactory::create_connection(int fd)
{
    return new Connection(fd);
}

void
ConnectionFactory::destroy_connection(Connection* conn)
{
    delete conn;
}

Pipeline::Pipeline()
{
    factory_ = new ConnectionFactory();
}

Pipeline::~Pipeline()
{
    delete factory_;
}

void
Pipeline::set_connection_factory(ConnectionFactory* fac)
{
    delete factory_;
    factory_ = fac;
}

Stage*
Pipeline::find_stage(const std::string& name)
{
    StageMap::iterator it = map_.find(name);
    if (it == map_.end()) {
        return NULL;
    } else {
        return it->second;
    }
}

Connection*
Pipeline::create_connection(int fd)
{
    return factory_->create_connection(fd);
}

void
Pipeline::dispose_connection(Connection* conn)
{
    LOG(DEBUG, "disposing connection %d %p", conn->fd, conn);
    StageMap::iterator it = map_.begin();
    Stage* stage = NULL;

    conn->lock();
    while (it != map_.end()) {
        stage = it->second;
        if (stage) {
            stage->sched_remove(conn);
        }
        ++it;
    }
    ::close(conn->fd);
    conn->unlock();
    factory_->destroy_connection(conn);
    LOG(DEBUG, "disposed");
}

void
Pipeline::add_stage(const std::string& name, Stage* stage)
{
    if (name == "poll_in") {
        poll_in_stage_ = (PollInStage*) stage;
    }
    map_.insert(std::make_pair(name, stage));
}

void
Pipeline::disable_poll(Connection* conn)
{
    poll_in_stage_->sched_remove(conn);
    utils::set_socket_blocking(conn->fd, true);
}

void
Pipeline::enable_poll(Connection* conn)
{
    utils::set_socket_blocking(conn->fd, false);
    if (!conn->inactive) {
        poll_in_stage_->sched_add(conn);
    }
}

void
Pipeline::reschedule_all()
{
    for (StageMap::iterator it = map_.begin(); it != map_.end(); ++it) {
        Stage* stage = it->second;
        stage->sched_reschedule();
    }
}

}

