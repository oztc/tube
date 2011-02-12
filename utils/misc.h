// -*- mode: c++ -*-

#ifndef _MISC_H_
#define _MISC_H_

#include <sys/types.h>
#include <fcntl.h>

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread.hpp>

namespace pipeserv {
namespace utils {

typedef boost::mutex Mutex;
typedef boost::unique_lock<Mutex> Lock;
typedef boost::condition Condition;

typedef boost::shared_mutex RWMutex;
typedef boost::shared_lock<RWMutex> SLock;
typedef boost::unique_lock<RWMutex> XLock;

typedef boost::thread Thread;

void set_socket_blocking(int fd, bool block);
void set_fdtable_size(size_t sz);
}
}

#endif /* _MISC_H_ */
