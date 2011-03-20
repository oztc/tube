#include "pch.h"

#include <unistd.h>
#include <sys/uio.h>
#include <cstdio>

#include "core/buffer.h"
#include "utils/exception.h"
#include "utils/logger.h"

using namespace pipeserv::utils;

namespace pipeserv {

const size_t Buffer::kPageSize = 8192;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ALLOC_PAGE() ((byte*) malloc(kPageSize))

Buffer::Buffer()
{
    cow_info_ = CowInfoPtr(new Buffer::CowInfo());
    borrowed_ = false;
    cow_info_->pages_.push_back(ALLOC_PAGE());
    left_offset_ = size_ = 0;
    right_offset_ = kPageSize;
}

Buffer::Buffer(const Buffer& rhs)
    : left_offset_(rhs.left_offset_), right_offset_(rhs.right_offset_),
      size_(rhs.size_)
{
    cow_info_ = rhs.cow_info_; // increase the reference
    borrowed_ = true;
}

Buffer&
Buffer::operator=(const Buffer& rhs)
{
    left_offset_ = rhs.left_offset_;
    right_offset_ = rhs.right_offset_;
    size_ = rhs.size_;

    cow_info_ = rhs.cow_info_;
    borrowed_ = true;
    return *this;
}

Buffer::CowInfo::CowInfo()
{
    extra_page_ = ALLOC_PAGE();
}

Buffer::CowInfo::~CowInfo()
{
    free(extra_page_);
    for (PageList::iterator it = pages_.begin(); it != pages_.end(); ++it) {
        free(*it);
    }
}

void
Buffer::copy_for_write()
{
    // puts("copy for write");
    CowInfoPtr ptr = cow_info_;
    cow_info_ = CowInfoPtr(new CowInfo());
    borrowed_ = false;

    for (PageList::const_iterator it = ptr->pages_.begin();
         it != ptr->pages_.end(); ++it) {
        byte* page_data = ALLOC_PAGE();
        memcpy(page_data, *it, kPageSize);
        cow_info_->pages_.push_back(page_data);
    }
}

bool
Buffer::need_copy_for_write() const
{
    return borrowed_ && cow_info_.use_count() > 1;
}

Buffer::~Buffer()
{
}

ssize_t
Buffer::read_from_fd(int fd)
{
    struct iovec vec[2];
    if (need_copy_for_write())
        copy_for_write();

    vec[0].iov_base = cow_info_->pages_.back() + kPageSize - right_offset_;
    vec[0].iov_len = right_offset_;
    vec[1].iov_base = cow_info_->extra_page_;
    vec[1].iov_len = kPageSize;

    int nread = readv(fd, vec, 2);
    if (nread < 0)
        return nread;
    if (right_offset_ <= (size_t) nread) {
        right_offset_ = kPageSize + right_offset_ - nread;
        cow_info_->pages_.push_back(cow_info_->extra_page_);
        cow_info_->extra_page_ = ALLOC_PAGE();
    } else {
        right_offset_ -= nread;
    }
    size_ += nread;
    return nread;
}

bool
Buffer::append(const byte* ptr, size_t sz)
{
    if (need_copy_for_write())
        copy_for_write();

    size_ += sz;

    byte* dest = cow_info_->pages_.back() + kPageSize - right_offset_;
    if (sz < right_offset_) {
        memcpy(dest, ptr, sz);
        right_offset_ -= sz;
        return true;
    }
    memcpy(dest, ptr, right_offset_);
    ptr += right_offset_;
    sz -= right_offset_;
    while (true) {
        dest = ALLOC_PAGE();
        memcpy(dest, ptr, MIN(kPageSize, sz));
        cow_info_->pages_.push_back(dest);
        if (sz < kPageSize) {
            right_offset_ = kPageSize - sz;
            break;
        }
        ptr += kPageSize;
        sz -= kPageSize;
    }
    return true; // buffer objects always accept the append operation
}

bool
Buffer::copy_front(byte* ptr, size_t sz)
{
    if (size_ < sz)
        return false;

    PageIterator it = cow_info_->pages_.begin();
    int ncopy = 0;
    do {
        if (it == cow_info_->pages_.begin()) {
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
    if (need_copy_for_write())
        copy_for_write();

    if (size_ < pop_size)
        return false;
    div_t res = div(pop_size + left_offset_, kPageSize);
    int npage = res.quot;
    left_offset_ = res.rem;
    for (int i = 0; i < npage; i++) {
        if (cow_info_->pages_.size() > 1) {
            free(cow_info_->pages_.front());
            cow_info_->pages_.pop_front();
        }
    }
    size_ -= pop_size;
    if (size_ == 0) {
        left_offset_ = 0;
        right_offset_ = kPageSize;
    }
    return true;
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

ssize_t
Buffer::write_to_fd(int fd)
{
    if (need_copy_for_write())
        copy_for_write();

    if (size_ == 0)
        return 0;
    int nwrite = 0;
    struct iovec vec[2];
    PageList::iterator it = cow_info_->pages_.begin();
    int nvec = MIN(2, cow_info_->pages_.size());
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
    }
    return nwrite;
}

byte*
Buffer::get_page_segment(byte* page_start_ptr, size_t* len_ret)
{
    byte* ptr = page_start_ptr;
    size_t len = kPageSize;
    if (page_start_ptr == cow_info_->pages_.front()) {
        ptr += left_offset_;
        len -= left_offset_;
    }
    if (page_start_ptr == cow_info_->pages_.back()) {
        len -= right_offset_;
    }
    if (len_ret) *len_ret = len;
    return ptr;
}

}
