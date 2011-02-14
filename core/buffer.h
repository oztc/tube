// -*- mode: c++ -*-

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <cstdlib>
#include <stdint.h>
#include <list>

namespace pipeserv {

typedef uint8_t byte;

class Buffer
{
    static const size_t kPageSize;
public:
    typedef std::list<byte*> PageList;
    typedef PageList::iterator PageIterator;

    Buffer();
    virtual ~Buffer();

    size_t size() const { return size_; }

    int read_from_fd(int fd);
    int write_to_fd(int fd);

    void append(const byte* ptr, size_t sz);
    bool copy_front(byte* ptr, size_t sz);
    bool pop(size_t pop_size);
    int  pop_page();
    void clear();

    PageIterator page_begin() { return pages_.begin(); }
    PageIterator page_end() { return pages_.end(); }

    // refine page segment according to page start pointer
    byte* first_page() const { return pages_.front(); }
    byte* last_page() const { return pages_.back(); }
    byte* get_page_segment(byte* page_start_ptr, size_t* len_ret);

private:
    byte*    extra_page_;
    PageList pages_;

    size_t   left_offset_, right_offset_;
    size_t   size_;
};

}

#endif /* _BUFFER_H_ */
