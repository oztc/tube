#include <unistd.h>
#include <sys/uio.h>
#include <cstdio>

#include "core/buffer.h"
#include "utils/exception.h"

using namespace pipeserv::utils;

namespace pipeserv {

const size_t Buffer::kPageSize = 8192;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ALLOC_PAGE() ((byte*) malloc(kPageSize))

Buffer::Buffer()
{
    extra_page_ = ALLOC_PAGE();
    pages_.push_back(ALLOC_PAGE());
    left_offset_ = size_ = 0;
    right_offset_ = kPageSize;
}

Buffer::~Buffer()
{
    free(extra_page_);
    for (PageList::iterator it = pages_.begin(); it != pages_.end(); ++it) {
        free(*it);
    }
}

int
Buffer::read_from_fd(int fd)
{
    byte* last_page = pages_.back();
    size_t last_offset = kPageSize - right_offset_;
    struct iovec vec[2];
    int nread = 0;

    vec[0].iov_base = last_page + last_offset;
    vec[0].iov_len = right_offset_;
    vec[1].iov_base = extra_page_;
    vec[1].iov_len = kPageSize;
    while ((nread = readv(fd, vec, 2)) > 0) {
        if (right_offset_ <= nread) {
            right_offset_ = kPageSize + right_offset_ - nread;
            pages_.push_back(extra_page_);
            extra_page_ = ALLOC_PAGE();
        } else {
            right_offset_ -= nread;
        }
        size_ += nread;
    }
    return nread;
}

void
Buffer::append(const byte* ptr, size_t sz)
{
    size_ += sz;

    byte* dest = pages_.back() + kPageSize - right_offset_;
    int ndest = right_offset_;
    int ncopy = 0;
    while (true) {
        ncopy = MIN(sz, ndest);
        memcpy(dest, ptr, ncopy);
        ptr += ncopy;
        sz -= ncopy;
        if (sz > 0) {
            dest = ALLOC_PAGE();
            ndest = kPageSize;
            pages_.push_back(dest);
        } else {
            break;
        }
    }
    right_offset_ = kPageSize - ncopy;
}

bool
Buffer::copy_front(byte* ptr, size_t sz)
{
    if (size_ < sz)
        return false;

    PageIterator it = pages_.begin();
    int ncopy = 0;
    do {
        if (it == pages_.begin()) {
            ncopy = MIN(sz, kPageSize - left_offset_);
            memcpy(ptr, *it + left_offset_, ncopy);
        } else {
            ncopy = MIN(sz, kPageSize);
            memcpy(ptr, *it, ncopy);
        }
        ptr += sz;
        sz -= ncopy;
        ++it;
    } while (sz > 0);
    return true;
}

bool
Buffer::pop(size_t pop_size)
{
    if (size_ < pop_size)
        return false;
    div_t res = div(pop_size + left_offset_, kPageSize);
    int npage = res.quot;
    left_offset_ = res.rem;
    for (int i = 0; i < npage; i++) {
        if (pages_.size() > 1) {
            free(pages_.front());
            pages_.pop_front();
        }
    }
    size_ -= pop_size;
    if (size_ == 0) {
        left_offset_ = 0;
        right_offset_ = kPageSize;
    }
}

int
Buffer::pop_page()
{
    int buf_size = kPageSize - left_offset_;
    pop(buf_size);
    return buf_size;
}

void
Buffer::clear()
{
    pop(size_);
}

int
Buffer::write_to_fd(int fd)
{
    if (size_ == 0)
        return 0;
    int nwrite = 0;
    struct iovec vec[2];
    while (true) {
        PageList::iterator it = pages_.begin();
        int nvec = MIN(2, pages_.size());
        for (int i = 0; i < nvec; i++, ++it) {
            byte* ptr = *it;
            if (i == 0)
                ptr += left_offset_;
            vec[i].iov_base = ptr;
            vec[i].iov_len = kPageSize;
        }
        vec[0].iov_len -= left_offset_;
        vec[nvec - 1].iov_len -= right_offset_;
        nwrite = writev(fd, vec, nvec);
        if (nwrite > 0) {
            pop(nwrite);
        } else {
            break;
        }
    }
    return nwrite;
}

byte*
Buffer::get_page_segment(byte* page_start_ptr, size_t* len_ret)
{
    byte* ptr = page_start_ptr;
    size_t len = kPageSize;
    if (page_start_ptr == pages_.front()) {
        ptr += left_offset_;
        len -= left_offset_;
    }
    if (page_start_ptr == pages_.back()) {
        len -= right_offset_;
    }
    if (len_ret) *len_ret = len;
    return ptr;
}

}
