// -*- mode: c++ -*-
#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

#include <stdexcept>
#include <sstream>
#include <string>
#include <cstring>
#include <errno.h>

namespace pipeserv {
namespace utils {

class SyscallException : public std::exception
{
    int err_;
public:
    SyscallException() : err_(errno) {}

    virtual const char* what() const throw() {
        return strerror(err_);
    }
};

class UnrecognizedAddress : public std::exception
{
public:
    virtual const char* what() const throw() {
        return "Unrecognized address family";
    }
};

class BufferFullException : public std::exception
{
    std::string msg_;
public:
    BufferFullException(int max_size) {
        std::stringstream ss;
        ss << "buffer size reached limit " << max_size;
        msg_ = ss.str();
    }

    ~BufferFullException() throw() {}

    virtual const char* what() const throw() {
        return msg_.c_str();
    }
};

}
}

#endif /* _EXCEPTION_H_ */
