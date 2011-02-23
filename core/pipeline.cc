#include <ctime>
#include <unistd.h>

#include "core/pipeline.h"
#include "core/stages.h"
#include "utils/logger.h"
#include "utils/misc.h"

namespace pipeserv {

Connection::Connection()
{
    timeout = 0; // default no timeout
    prio = 0;
    inactive = false;
    last_active = time(NULL);
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

QueueScheduler::QueueScheduler(bool supress_connection_lock)
    : Scheduler(), supress_connection_lock_(supress_connection_lock)
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

    size_t len = list_.size();
    Connection* conn = NULL;
    for (NodeList::iterator it = list_.begin(); it != list_.end(); ++it) {
        conn = *it;
        if (conn->trylock()) {
            list_.erase(it);
            nodes_.erase(conn);
            return conn;
        }
    }
    conn = list_.front();
    conn->lock();
    list_.pop_front();
    nodes_.erase(conn);
    return conn;
}

Connection*
QueueScheduler::pick_task()
{
    if (supress_connection_lock_) {
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
ConnectionFactory::create_connection()
{
    return new Connection();
}

void
ConnectionFactory::destroy_connection(Connection* conn)
{
    delete conn;
}

Connection*
Pipeline::create_connection()
{
    return factory_->create_connection();
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
Pipeline::add_stage(std::string name, Stage* stage)
{
    if (name == "poll_in") {
        poll_in_stage_ = stage;
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

}

