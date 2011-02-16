// -*- mode: c++ -*-

#ifndef _POLLER_H_
#define _POLLER_H_

#include <string>
#include <google/dense_hash_set>
#include <google/sparse_hash_map>
#include <boost/function.hpp>

#include "utils/misc.h"

namespace pipeserv {

class Connection;

typedef unsigned short PollerEvent;

static const PollerEvent POLLER_EVENT_READ  = 1;
static const PollerEvent POLLER_EVENT_WRITE = 2;
static const PollerEvent POLLER_EVENT_ERROR = 4;
static const PollerEvent POLLER_EVENT_HUP   = 8;

class Poller : public utils::Noncopyable
{
public:
    typedef boost::function<void (Connection*, PollerEvent)> PollerCallback;

    Poller() throw();
    virtual ~Poller() {}

    void set_handler(const PollerCallback& cb) {
        handler_ = cb;
    }

    size_t size() {
        return fds_.size();
    }

    bool has_fd(int fd) const {
        if (fds_.find(fd) == fds_.end())
            return false;
        return true;
    }

    virtual void handle_event(int timeout) throw() = 0;
    virtual bool add_fd(int fd, Connection* conn, PollerEvent evt) = 0;
    virtual bool remove_fd(int fd) = 0;
protected:
    typedef google::dense_hash_set<int> FDSet;

    FDSet          fds_;
    // handler when events happened
    PollerCallback handler_;
protected:
    bool add_fd_set(int fd) {
        return fds_.insert(fd).second;
    }

    bool remove_fd_set(int fd) {
        return fds_.erase(fd);
    }
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

    Poller* create_poller(std::string name);
    void    destroy_poller(Poller* poller);
private:
    typedef google::sparse_hash_map<std::string, CreateFunc> PollerMap;
    PollerMap poller_map_;
};

}

#endif /* _POLLER_H_ */

