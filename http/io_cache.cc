#include "io_cache.h"

namespace pipeserv {

const size_t IOCacheEntry::kCacheEntryMaxSize = 4096;

IOCacheEntry::IOCacheEntry(const std::string& file_path, time_t mtime,
                           size_t file_size)
    : path(file_path), last_mtime(0), file_content(NULL), size(file_size)
{
    int file_desc = ::open(path.c_str(), O_RDONLY);
    if (file_desc < 0)
        return;
    file_content = new byte[file_size];
    byte* buf = file_content;
    while (true) {
        // file_size should be small though
        ssize_t nread = ::read(file_desc, buf, file_size);
        if (nread == 0) {
            break;
        } else if (nread < 0) {
            // read error
            delete [] file_content;
            file_content = NULL;
            return;
        }
        buf += nread;
    }
    last_mtime = mtime;
    ::close(file_desc);
}

IOCacheEntry::~IOCacheEntry()
{
    last_mtime = 0;
    delete [] file_content;
}

byte*
IOCacheEntry::data_copy()
{
    byte* data = new byte[size];
    memcpy(data, file_content, size);
    return data;
}

IOCache::IOCache()
    : max_cache_entry_(0), max_entry_size_(4096) // 4K by default
{
}

void
IOCache::add_entry(IOCacheEntry* entry)
{
    entries_.push_front(entry);
    entry_map_.insert(std::make_pair(entry->path, entries_.begin()));
}

void
IOCache::move_entry(EntryMap::iterator map_it)
{
    IOCacheEntry* entry = *(map_it->second);
    entries_.erase(map_it->second);
    entry_map_.erase(map_it);
    entries_.push_front(entry);
    entry_map_.insert(std::make_pair(entry->path, entries_.begin()));
}

void
IOCache::drop_entry()
{
    EntryList::iterator it = entries_.end();
    --it;
    IOCacheEntry* entry = *it;

    entries_.erase(it);
    entry_map_.erase(entry->path);
    delete entry;
}

void
IOCache::remove_entry(EntryMap::iterator map_it)
{
    IOCacheEntry* entry = *(map_it->second);
    entries_.erase(map_it->second);
    entry_map_.erase(map_it);
    delete entry;
}

IOCacheEntry*
IOCache::create_cache_entry(const std::string& file_path, time_t mtime,
                            size_t file_size)
{
    if (file_size >= max_entry_size_)
        return NULL;
    IOCacheEntry* entry = new IOCacheEntry(file_path, mtime, file_size);
    if (entry->file_content == NULL) {
        delete entry;
        entry = NULL;
    }
    return entry;
}

byte*
IOCache::sync_cache(EntryMap::iterator it, const std::string& file_path,
                    time_t mtime, size_t file_size)
{
    IOCacheEntry* entry = *(it->second);
    if (entry->last_mtime >= mtime && entry->last_mtime
        && entry->size == file_size) {
        move_entry(it);
        byte* ret = entry->data_copy();
        mutex_.unlock();
        return ret;
    }
    remove_entry(it);

    return load_cache(file_path, mtime, file_size);
}

byte*
IOCache::load_cache(const std::string& file_path, time_t mtime,
                    size_t file_size)
{
    // load cache
    if (entries_.size() >= max_cache_entry_) {
        drop_entry();
    }

    mutex_.unlock();
    IOCacheEntry* entry = create_cache_entry(file_path, mtime, file_size);
    if (entry == NULL) {
        return NULL;
    }
    utils::Lock lk(mutex_);
    add_entry(entry);
    return entry->data_copy();
}

byte*
IOCache::access_cache(const std::string& file_path, time_t mtime,
                      size_t file_size)
{
    if (max_cache_entry_ == 0)
        return NULL;
    mutex_.lock();
    EntryMap::iterator it = entry_map_.find(file_path);
    if (it == entry_map_.end()) {
        return load_cache(file_path, mtime, file_size);
    } else {
        return sync_cache(it, file_path, mtime, file_size);
    }
}

}
