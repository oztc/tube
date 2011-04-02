// -*- mode: c++ -*-
#ifndef _FILESENDER_H_
#define _FILESENDER_H_

#include <cstring>
#include <unistd.h>
#include <sys/types.h>

#include "core/buffer.h"

namespace tube {

class FileSender : public Writeable
{
    int         file_fd_;
    off64_t     offset_;
    off64_t     length_;
public:
    FileSender(int file_desc, off64_t offset, off64_t length);
    virtual ~FileSender();

    virtual ssize_t write_to_fd(int fd);
    virtual u64     size() const { return length_; }
    virtual size_t  memory_usage() const { return 0; }
    virtual bool    append(const byte* data, size_t size) { return false; }
};

}

#endif /* _FILESENDER_H_ */
