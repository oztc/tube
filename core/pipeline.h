// -*- mode: c++ -*-

#ifndef _PIPELINE_H_
#define _PIPELINE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <list>
#include <string>
#include <set>

#include "utils/misc.h"
#include "core/buffer.h"

namespace pipeserv {

class Connection
{
public:
    sockaddr* addr;
    socklen_t addr_len;
    int       fd;
    int       prio;
    bool      inactive;

    // buffers
    Buffer    in_buf;
    Buffer    out_buf;

    // locks
    bool         hold;
    utils::Mutex mutex;

    bool trylock();
    void lock();
    void unlock();
};

class Scheduler
{
public:
    Scheduler();
    virtual ~Scheduler();

    virtual void add_task(Connection* conn)    = 0;
    virtual Connection* pick_task()            = 0;
    virtual void remove_task(Connection* conn) = 0;
};

class QueueScheduler : public Scheduler
{
protected:
    typedef std::list<Connection*> NodeList;
    typedef std::map<Connection*, NodeList::iterator> NodeMap;

    NodeList  list_;
    NodeMap   nodes_;

    utils::Mutex      mutex_;
    utils::Condition  cond_;
public:
    QueueScheduler();
    ~QueueScheduler();

    virtual void        add_task(Connection* conn);
    virtual Connection* pick_task();
    virtual void        remove_task(Connection* conn);
};

class Stage;

struct ConnectionFactory
{
public:
    virtual Connection* create_connection();
    virtual void        destroy_connection(Connection* conn);
};

class Pipeline
{
    typedef std::map<std::string, Stage*> StageMap;
    StageMap       map_;
    utils::RWMutex mutex_;

    Stage*             poll_in_stage_;
    ConnectionFactory* factory_;

    Pipeline() {
        factory_ = new ConnectionFactory();
    }

    ~Pipeline() {
        delete factory_;
    }

public:
    static Pipeline& instance() {
        static Pipeline ins;
        return ins;
    }

    utils::RWMutex& mutex() { return mutex_; }

    void add_stage(std::string name, Stage* stage);

    void set_connection_factory(ConnectionFactory* fac) {
        delete factory_;
        factory_ = fac;
    }

    Stage* find_stage(std::string name) {
        StageMap::iterator it = map_.find(name);
        if (it == map_.end()) {
            return NULL;
        } else {
            return it->second;
        }
    }

    Connection* create_connection();
    void dispose_connection(Connection* conn);

    void disable_poll(Connection* conn);
    void enable_poll(Connection* conn);
};

}

#endif /* _PIPELINE_H_ */
