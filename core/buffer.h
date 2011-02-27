// -*- mode: c++ -*-

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <cstdlib>
#include <stdint.h>
#include <list>

#include <sys/types.h>
#include <boost/shared_ptr.hpp>

#include "utils/misc.h"

namespace pipeserv {

class Writeable
{
public:
    virtual ~Writeable() {}

    virtual ssize_t write_to_fd(int fd) = 0;
    virtual size_t  size() const = 0;
    virtual size_t  memory_usage() const = 0;
    virtual bool    append(const byte* ptr, size_t size) = 0;
};

class Buffer : public Writeable
{
public:
    static const size_t kPageSize;

    typedef std::list<byte*> PageList;
    typedef PageList::iterator PageIterator;

    Buffer();
    Buffer(const Buffer& rhs);
    virtual ~Buffer();

    Buffer& operator=(const Buffer& rhs);

    virtual size_t size() const { return size_; }
    virtual size_t memory_usage() const { return size_; }

    ssize_t read_from_fd(int fd);

    virtual ssize_t write_to_fd(int fd);
    virtual bool    append(const byte* ptr, size_t sz);

    bool copy_front(byte* ptr, size_t sz);
    bool pop(size_t pop_size);
    int  pop_page();
    void clear();

    PageIterator page_begin() { return cow_info_->pages_.begin(); }
    PageIterator page_end() { return cow_info_->pages_.end(); }

    // refine page segment according to page start pointer
    byte* first_page() const { return cow_info_->pages_.front(); }
    byte* last_page() const { return cow_info_->pages_.back(); }
    byte* get_page_segment(byte* page_start_ptr, size_t* len_ret);

private:
    void copy_for_write();
    bool need_copy_for_write() const;
    void new_cow_info();

private:
    struct CowInfo {
        CowInfo();
        ~CowInfo();

        byte*    extra_page_;
        PageList pages_;
    };

    typedef boost::shared_ptr<CowInfo> CowInfoPtr;

    CowInfoPtr cow_info_;
    bool       borrowed_;

    size_t   left_offset_, right_offset_;
    size_t   size_;
};

}

#endif /* _BUFFER_H_ */
