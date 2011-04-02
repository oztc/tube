// -*- mode: c++ -*-

#ifndef _WRAPPER_H_
#define _WRAPPER_H_

#include <sys/types.h>
#include <unistd.h>

#include "pipeline.h"

namespace tube {

class Stage;

class Wrapper
{
protected:
    Connection* conn_;
    Pipeline&   pipeline_;
public:
    Wrapper(Connection* conn);
    virtual ~Wrapper() {}
protected:
    void disable_poll() { pipeline_.disable_poll(conn_); }
    void enable_poll() { pipeline_.enable_poll(conn_); }
};

class Request : public Wrapper
{
public:
    Request(Connection* conn);
    virtual ~Request();

    ssize_t read_data(byte* ptr, size_t sz);
};

class Response : public Wrapper
{
protected:
    size_t      max_mem_;
    bool        inactive_;
    Stage*      out_stage_;
    size_t      total_mem_;
public:
    static size_t kMaxMemorySizes;

    Response(Connection* conn);
    virtual ~Response();

    int     response_code() const;

    virtual ssize_t write_data(const byte* ptr, size_t sz);
    virtual ssize_t write_string(std::string str);
    virtual ssize_t write_string(const char* str);
    virtual void    write_file(int file_desc, off64_t offset, off64_t length);
    virtual ssize_t flush_data();

    bool    active() const { return !inactive_; }
    void    close();
};

}

#endif /* _WRAPPER_H_ */
