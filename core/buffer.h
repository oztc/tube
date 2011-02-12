// -*- mode: c++ -*-

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <cstdlib>
#include <stdint.h>
#include <vector>

namespace pipeserv {

typedef uint8_t byte;

class Buffer
{
    std::vector<byte> vec_;
    int max_size_;
public:
    Buffer();
    virtual ~Buffer();

    void set_max_size(int max_size) { max_size_ = max_size; }
    int  max_size() { return max_size_; }

    void append(const byte* ptr, size_t sz);
    void pop(size_t sz);
    void clear();

    size_t size() { return vec_.size(); }

    byte operator[](int idx) const { return vec_[idx]; }
    const byte* ptr() const { return &vec_[0]; }
};

}

#endif /* _BUFFER_H_ */
