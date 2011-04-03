// -*- mode: c++ -*-

#ifndef _FDMAP_H_
#define _FDMAP_H_

#include <sys/time.h>
#include <sys/resource.h>
#include <boost/shared_array.hpp>
#include <cassert>

namespace tube {
namespace utils {

// Why won't we use a generic hashmap?
// * portable and high performance implementation is hard to find
// * max file descriptor isn't that large
// * generic hashmap needs to keep the density as a const factor as possible
//   which would waste space

inline size_t
get_fdmap_max_size()
{
    struct rlimit lmt;
    if (getrlimit(RLIMIT_NOFILE, &lmt) < 0) {
        return 32767;
    } else {
        return (size_t) lmt.rlim_cur;
    }    
}

template <typename T>
class FDMap
{
public:
    typedef typename std::list<T> ItemList;
    typedef typename ItemList::iterator iterator;
    typedef typename ItemList::const_iterator const_iterator;

    FDMap() {
        max_size_ = get_fdmap_max_size();
        table_ = boost::shared_array<iterator>(new iterator[max_size_]);
        for (size_t i = 0; i < max_size_; i++) {
            table_[i] = items_.end(); // all set to invalid iterator
        }
    }
    virtual ~FDMap() {}

    iterator begin() { return items_.begin(); }
    iterator end() { return items_.end(); }
    const_iterator begin() const { return items_.begin(); }
    const_iterator end() const { return items_.end(); }
    size_t size() const { return items_.size(); }

    iterator find(size_t idx) const {
        assert(idx < max_size_);
        return table_[idx];
    }

    bool erase(size_t idx) {
        assert(idx < max_size_);
        if (table_[idx] == end()) {
            return false;
        } else {
            items_.erase(table_[idx]);
            table_[idx] = end();
            return true;
        }
    }

    bool insert(size_t idx, const T& value) {
        assert(idx < max_size_);
        if (table_[idx] == end()) {
            table_[idx] = items_.insert(items_.end(), value);
            return true;
        } else {
            return false;
        }
    }
private:
    size_t max_size_;

    ItemList                      items_;
    boost::shared_array<iterator> table_;
};

}
}

#endif /* _FDMAP_H_ */
