// -*- mode: c++ -*-

#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <list>
#include <string>
#include <set>

#include <map>
#include <google/dense_hash_map>

#include "utils/misc.h"
#include "core/buffer.h"
#include "core/inet_address.h"

namespace pipeserv {

struct Connection
{
    volatile uint32_t last_active;
    volatile uint32_t timeout;

    int       fd;
    int       prio;
    bool      inactive;

    InternetAddress address;

    // buffers
    Buffer    in_buf;
    Buffer    out_buf;

    // locks
    utils::Mutex mutex;
    long         owner;


    bool trylock();
    void lock();
    void unlock();

    std::string address_string() const;
    void set_timeout(int sec) { timeout = sec; }

    Connection();
};

class Scheduler : utils::Noncopyable
{
public:
    Scheduler();
    virtual ~Scheduler();

    virtual void add_task(Connection* conn)     = 0;
    virtual Connection* pick_task()             = 0;
    virtual void remove_task(Connection* conn)  = 0;
};

class QueueScheduler : public Scheduler
{
protected:
    typedef std::list<Connection*> NodeList;
    typedef google::dense_hash_map<Connection*, NodeList::iterator,
                                   utils::PtrHashFunc> NodeMap;
    //typedef std::map<Connection*, NodeList::iterator> NodeMap;

    NodeList  list_;
    NodeMap   nodes_;

    utils::Mutex      mutex_;
    utils::Condition  cond_;

    bool      supress_connection_lock_;
public:
    QueueScheduler(bool supress_connection_lock = false);
    ~QueueScheduler();

    virtual void        add_task(Connection* conn);
    virtual Connection* pick_task();
    virtual void        remove_task(Connection* conn);
private:
    Connection* pick_task_nolock_connection();
    Connection* pick_task_lock_connection();
};

class Stage;

struct ConnectionFactory
{
public:
    virtual Connection* create_connection();
    virtual void        destroy_connection(Connection* conn);
};

class Pipeline : utils::Noncopyable
{
    typedef std::map<std::string, Stage*> StageMap;
    StageMap       map_;
    utils::RWMutex mutex_;

    Stage*             poll_in_stage_;
    ConnectionFactory* factory_;

    Pipeline();
    ~Pipeline();

public:
    static Pipeline& instance() {
        static Pipeline ins;
        return ins;
    }

    utils::RWMutex& mutex() { return mutex_; }

    void add_stage(std::string name, Stage* stage);

    void set_connection_factory(ConnectionFactory* fac);

    Stage* find_stage(std::string name);

    Connection* create_connection();
    void dispose_connection(Connection* conn);

    void disable_poll(Connection* conn);
    void enable_poll(Connection* conn);
};

}

#endif /* _PIPELINE_H_ */
