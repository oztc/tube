#include "pch.h"

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

#ifdef USE_LINUX_SENDFILE
#include <sys/sendfile.h>
#else
#ifdef USER_FREEBSD_SENDFILE
#include <sys/socket.h>
#include <sys/uio.h>
#else
#include <sys/mman.h>
#endif
#endif

#include "filesender.h"
#include "utils/exception.h"
#include "utils/logger.h"

namespace pipeserv {

FileSender::FileSender(int file_desc, off64_t offset, off64_t length)
    : file_fd_(file_desc), offset_(offset), length_(length)
{
    if (length_ == -1) {
        // get the length of whole file
        struct stat64 st;
        fstat64(file_fd_, &st);
        length_ = st.st_size - offset;
    }
    // if (::lseek64(file_fd_, offset_, SEEK_SET) < 0) {
    //     throw utils::SyscallException();
    // }
}

FileSender::~FileSender()
{
    ::close(file_fd_);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))

ssize_t
FileSender::write_to_fd(int fd)
{
    size_t n_should_send = MIN(length_, Buffer::kPageSize);
    ssize_t nsend = -1;
#ifdef USE_LINUX_SENDFILE
    // use sendfile64 under linux to send
    nsend =  sendfile64(fd, file_fd_, &offset_, n_should_send);
#else
#ifdef USE_FREEBSD_SENDFILE
    // use sendfile under freebsd to send
#else
    // use mmap and write to send
#endif
#endif
    if (nsend < 0)
        return nsend;
    length_ -= nsend;
    return nsend;
}

}
