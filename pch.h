#ifndef _PCH_H_
#define _PCH_H_

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include <set>
#include <map>
#include <list>
#include <string>
#include <queue>
#include <vector>

#include <fstream>
#include <sstream>

#include <yaml-cpp/yaml.h>

#include <boost/shared_ptr.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

// Must disable assert, because pthread_mutex_unlock on BSD will return an error
// when mutex is locked by a different thread.
#define BOOST_DISABLE_ASSERTS

#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>

#endif /* _PCH_H_ */
