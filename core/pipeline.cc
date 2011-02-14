#include "core/pipeline.h"
#include "core/stages.h"
#include "utils/logger.h"
#include "utils/misc.h"

namespace pipeserv {

bool
Connection::trylock()
{
    return mutex.try_lock();
}

void
Connection::lock()
{
    mutex.lock();
}

void
Connection::unlock()
{
    mutex.unlock();
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
        return;
    }
    bool need_notify = (list_.size() == 0);
    list_.push_back(conn);
    NodeList::iterator node = list_.end();
    nodes_.insert(std::make_pair(conn, --node));

    lk.unlock();
    cond_.notify_all();
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
QueueScheduler::pick_task()
{
    QueueSchedulerPickScope lk(mutex_);
    while (list_.empty()) {
        cond_.wait(lk);
    }
    size_t len = list_.size();
    Connection* conn = NULL;
    for (size_t i = 0; i <= len; i++) {
        conn = list_.front();
        list_.pop_front();
        if (supress_connection_lock_ || conn->trylock()) {
            nodes_.erase(conn);
            break;
        }
        if (i == len) {
            if (!supress_connection_lock_)
                conn->lock();
            nodes_.erase(conn);
            break;
        }
        list_.push_back(conn);
        NodeList::iterator it = list_.end();
        nodes_[conn] = --it;
    }
    return conn;
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
    free(conn->addr);
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
    close(conn->fd);
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

