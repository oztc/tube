// -*- mode: c++ -*-

#ifndef _IO_CACHE_H_
#define _IO_CACHE_H_

#include <string>
#include <map>
#include <ctime>

#include "utils/misc.h"

namespace tube {

struct IOCacheEntry
{
    static const size_t kCacheEntryMaxSize;
    std::string path;
    time_t last_mtime; // size and extra stuff are synchronized every request
    byte*  file_content;
    size_t size;

    IOCacheEntry(const std::string& file_path, time_t mtime, size_t file_size);
    virtual ~IOCacheEntry();

    byte* data_copy();
};

class IOCache
{
    typedef std::list<IOCacheEntry*> EntryList;
    typedef std::map<std::string, EntryList::iterator> EntryMap;

    size_t max_cache_entry_;
    size_t max_entry_size_;

    EntryList entries_;
    EntryMap entry_map_;

    utils::Mutex mutex_;
public:
    IOCache();

    void set_max_cache_entry(size_t nentry) { max_cache_entry_ = nentry; }
    void set_max_entry_size(size_t size) { max_entry_size_ = size; }

    byte* access_cache(const std::string& file_path, time_t mtime,
                       size_t file_size);
private:
    IOCacheEntry* create_cache_entry(const std::string& file_path, time_t mtime,
                                     size_t file_size);

    byte* load_cache(const std::string& file_path, time_t mtime,
                     size_t file_size);

    byte* sync_cache(EntryMap::iterator it, const std::string& file_path,
                     time_t mtime, size_t file_size);

    void add_entry(IOCacheEntry* entry);
    void move_entry(EntryMap::iterator map_it);
    void drop_entry();
    void remove_entry(EntryMap::iterator map_it);
};

}

#endif /* _IO_CACHE_H_ */
