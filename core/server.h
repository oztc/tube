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
protected:
    PollInStage* read_stage_;
    WriteBackStage* out_stage_;
    RecycleStage* recycle_stage_;
public:
    Server(const char* host, const char* service) throw();
    virtual ~Server();

    void initialize_stages();
    void listen(int queue_size);
    void main_loop();

    int fd() const { return fd_; }
};

}

#endif /* _SERVER_H_ */
