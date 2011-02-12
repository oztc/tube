#include "core/buffer.h"
#include "utils/exception.h"

using namespace pipeserv::utils;

namespace pipeserv {

#define DEFAULT_MAX_BUFFER (64 << 20) // 64M
#define INITIAL_BUFFER_SIZE 512


Buffer::Buffer()
{
    max_size_ = DEFAULT_MAX_BUFFER;
    vec_.reserve(INITIAL_BUFFER_SIZE);
}

Buffer::~Buffer() {}

void
Buffer::append(const byte* ptr, size_t sz)
{
    if (vec_.size() + sz > max_size_)
        throw BufferFullException(max_size_);
    vec_.insert(vec_.end(), ptr, ptr + sz);
}

void
Buffer::pop(size_t sz)
{
    if (vec_.size() < sz)
        sz = vec_.size();
    vec_.erase(vec_.begin(), vec_.begin() + sz);
}

void
Buffer::clear()
{
    vec_.clear();
}

}
