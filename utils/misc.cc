#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "utils/misc.h"
#include "utils/exception.h"

namespace pipeserv {
namespace utils {

void
set_socket_blocking(int fd, bool block)
{
    int flags = -1, origflags = -1;
    origflags = flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        throw SyscallException();
    if (block) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }
    if (origflags != flags && fcntl(fd, F_SETFL, flags) < 0)
        throw SyscallException();
}

void
set_fdtable_size(size_t size)
{
    struct rlimit lmt;
    lmt.rlim_cur = size;
    lmt.rlim_max = size;

    setrlimit(RLIMIT_NOFILE, &lmt);
}

long
get_thread_id()
{
#ifdef __linux__
    return syscall(SYS_gettid);
#else
    return (long) pthread_self();
#endif
}

}
}
