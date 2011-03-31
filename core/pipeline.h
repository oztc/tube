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
#include "core/stream.h"
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

    // input and output stream
    InputStream    in_stream;
    OutputStream   out_stream;

    // locks
    utils::Mutex mutex;
    long         owner;

    bool close_after_finish;

    bool trylock();
    void lock();
    void unlock();

    std::string address_string() const;
    void set_timeout(int sec) { timeout = sec; }
    void set_io_timeout(int msec);
    void set_cork();
    void clear_cork();

    void active_close();

    Connection(int sock);
    virtual ~Connection() {}
};

class Scheduler : utils::Noncopyable
{
public:
    Scheduler();
    virtual ~Scheduler();

    virtual void add_task(Connection* conn)     = 0;
    virtual Connection* pick_task()             = 0;
    virtual void remove_task(Connection* conn)  = 0;
    virtual void reschedule()                   = 0;
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

    bool      suppress_connection_lock_;
public:
    QueueScheduler(bool suppress_connection_lock = false);
    ~QueueScheduler();

    virtual void        add_task(Connection* conn);
    virtual Connection* pick_task();
    virtual void        remove_task(Connection* conn);
    virtual void        reschedule();
private:
    Connection* pick_task_nolock_connection();
    Connection* pick_task_lock_connection();
};

class Stage;

struct ConnectionFactory
{
public:
    virtual Connection* create_connection(int fd);
    virtual void        destroy_connection(Connection* conn);
};

class PollInStage;

class Pipeline : utils::Noncopyable
{
    typedef std::map<std::string, Stage*> StageMap;
    StageMap       map_;
    utils::RWMutex mutex_;

    PollInStage*             poll_in_stage_;
    ConnectionFactory* factory_;

    Pipeline();
    ~Pipeline();

public:
    static Pipeline& instance() {
        static Pipeline ins;
        return ins;
    }

    utils::RWMutex& mutex() { return mutex_; }

    void add_stage(const std::string& name, Stage* stage);

    void set_connection_factory(ConnectionFactory* fac);

    PollInStage* poll_in_stage() const { return poll_in_stage_; }
    Stage* find_stage(const std::string& name);

    Connection* create_connection(int fd);
    void dispose_connection(Connection* conn);

    void disable_poll(Connection* conn);
    void enable_poll(Connection* conn);

    void reschedule_all();
};

}

#endif /* _PIPELINE_H_ */
