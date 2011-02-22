// -*- mode: c++ -*-

#ifndef _SERVER_H_
#define _SERVER_H_

#include <sys/types.h>
#include <sys/socket.h>

#include "core/stages.h"

namespace pipeserv {

class Server
{
    int fd_;
    size_t addr_size_;

    size_t read_stage_cnt_, write_stage_cnt_;
    int    timeout_;
protected:
    PollInStage* read_stage_;
    WriteBackStage* write_stage_;
    RecycleStage* recycle_stage_;
public:
    static const size_t kDefaultReadStageCount;
    static const size_t kDefaultWriteStageCount;
    static const size_t kDefaultTimeout;
public:
    Server(const char* host, const char* service) throw();
    virtual ~Server();

    size_t read_stage_count() const { return read_stage_cnt_; }
    size_t write_stage_count() const { return write_stage_cnt_; }
    int    timeout() const { return timeout_; }

    void set_read_stage_count(size_t val) { read_stage_cnt_ = val; }
    void set_write_stage_count(size_t val) { write_stage_cnt_ = val; }
    void set_timeout(int sec) { timeout_ = sec; }

    void initialize_stages();
    void listen(int queue_size);
    void main_loop();

    int fd() const { return fd_; }
};

}

#endif /* _SERVER_H_ */
