// -*- mode: c++ -*-

#ifndef _SERVER_H_
#define _SERVER_H_

#include <sys/types.h>
#include <sys/socket.h>

#include "core/stages.h"

namespace tube {

class Server
{
    int fd_;
    size_t addr_size_;

    size_t read_stage_pool_size_;
    size_t write_stage_pool_size_;
protected:
    PollInStage* read_stage_;
    WriteBackStage* write_stage_;
    RecycleStage* recycle_stage_;
public:
    static const size_t kDefaultReadStagePoolSize;
    static const size_t kDefaultWriteStagePoolSize;
public:
    Server(const char* host, const char* service) ;
    virtual ~Server();

    int fd() const { return fd_; }

    void set_recycle_threshold(size_t threshold);
    void set_read_stage_pool_size(size_t val) { read_stage_pool_size_ = val; }
    void set_write_stage_pool_size(size_t val) { write_stage_pool_size_ = val; }

    void initialize_stages();
    void start_all_threads();

    void listen(int queue_size);
    void main_loop();
};

}

#endif /* _SERVER_H_ */
