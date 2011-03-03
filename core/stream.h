// -*- mode: c++ -*-

#ifndef _STREAM_H_
#define _STREAM_H_

#include "core/buffer.h"

namespace pipeserv {

class InputStream
{
public:
    InputStream(int fd) : fd_(fd) {}
    virtual ~InputStream() {}

    ssize_t read_into_buffer();
    Buffer& buffer() { return buffer_; }
    const Buffer& buffer() const { return buffer_; }
    void    close();

private:
    Buffer buffer_;
    int    fd_;
};

class OutputStream
{
public:
    OutputStream(int fd);
    virtual ~OutputStream();

    ssize_t write_into_output();
    void    append_data(const byte* data, size_t size);
    off64_t append_file(std::string filename, off64_t offset, off64_t length);
    bool    is_done() const { return writeables_.empty(); }
    size_t  memory_usage() const { return memory_usage_; }

private:
    std::list<Writeable*> writeables_;
    int                   fd_;
    size_t                memory_usage_;
};

}

#endif /* _STREAM_H_ */
