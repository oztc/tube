// -*- mode: c++ -*-

#ifndef _POLLER_H_
#define _POLLER_H_

#include <string>
#include <map>
#include <boost/function.hpp>

#include "utils/misc.h"
#include "utils/fdmap.h"

namespace tube {

class Connection;

typedef unsigned short PollerEvent;

static const PollerEvent POLLER_EVENT_READ  = 1;
static const PollerEvent POLLER_EVENT_WRITE = 2;
static const PollerEvent POLLER_EVENT_ERROR = 4;
static const PollerEvent POLLER_EVENT_HUP   = 8;

class Poller : public utils::Noncopyable
{
public:
    typedef boost::function<void (Connection*, PollerEvent)> EventCallback;
    typedef boost::function<void (Poller&)> PollerCallback;
    typedef utils::FDMap<Connection*> FDMap;

    Poller() ;
    virtual ~Poller() {}

    void set_event_handler(const EventCallback& cb) { handler_ = cb; }
    void set_pre_handler(const PollerCallback& cb) { pre_handler_ = cb; }
    void set_post_handler(const PollerCallback& cb) { post_handler_ = cb; }

    size_t size() const { return fds_.size(); }
    FDMap::const_iterator begin() const { return fds_.begin(); }
    FDMap::const_iterator end() const { return fds_.end(); }
    FDMap::iterator begin() { return fds_.begin(); }
    FDMap::iterator end() { return fds_.end(); }

    bool has_fd(int fd) const;
    Connection* find_connection(int fd);

    virtual void handle_event(int timeout)  = 0;
    virtual bool poll_add_fd(int fd, Connection* conn, PollerEvent evt) = 0;
    virtual bool poll_remove_fd(int fd) = 0;

    bool add_fd(int fd, Connection* conn, PollerEvent evt);
    bool remove_fd(int fd);
protected:
    FDMap          fds_;
    // handler when events happened
    EventCallback  handler_;
    // handler before or after events processed
    PollerCallback pre_handler_, post_handler_;
protected:
    bool add_fd_set(int fd, Connection* conn);
    bool remove_fd_set(int fd);
};

class PollerFactory : public utils::Noncopyable
{
    PollerFactory() {}
    ~PollerFactory() {}
public:
    static PollerFactory& instance() {
        static PollerFactory fac;
        return fac;
    }

    typedef boost::function<Poller* ()> CreateFunc;

    std::string default_poller_name() const;
    void        register_poller(const char* name, CreateFunc create_func);

    Poller* create_poller(const std::string& name);
    void    destroy_poller(Poller* poller);
private:
    typedef std::map<std::string, CreateFunc> PollerMap;
    PollerMap poller_map_;
};

#define EXPORT_POLLER_IMPL(name, klass)                                 \
    static Poller* name##_create() { return new klass(); }              \
    static void __##name##_construction() __attribute__((constructor)); \
    static void __##name##_construction() {                             \
        PollerFactory::instance().register_poller(#name, name##_create); \
    }                                                                   \


}

#endif /* _POLLER_H_ */

