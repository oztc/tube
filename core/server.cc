#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <cstdlib>
#include <signal.h>

#include "core/server.h"
#include "core/pipeline.h"
#include "core/stages.h"

#include "utils/exception.h"
#include "utils/logger.h"
#include "utils/misc.h"

using namespace pipeserv::utils;

namespace pipeserv {

static struct addrinfo*
lookup_addr(const char* host, const char* service)
{
    struct addrinfo hints;
    struct addrinfo* info;

    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC; // allow both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, service, &hints, &info) < 0) {
        throw SyscallException();
    }
    return info;
}

Server::Server(const char* host, const char* service) throw()
{
    struct addrinfo* info = lookup_addr(host, service);
    bool done = false;
    for (struct addrinfo* p = info; p != NULL; p = p->ai_next) {
        if ((fd_ = socket(p->ai_family, p->ai_socktype, 0)) < 0) {
            continue;
        }
        if (bind(fd_, p->ai_addr, p->ai_addrlen) < 0) {
            close(fd_);
            continue;
        }
        done = true;
        addr_size_ = p->ai_addrlen;
        break;
    }
    freeaddrinfo(info);
    if (!done) {
        std::string err = "Cannot bind port(service) ";
        err += service;
        err += " on host ";
        err += host;
        throw std::invalid_argument(err);
    }

    read_stage_ = new PollInStage();
    out_stage_ = new WriteBackStage();
    recycle_stage_ = new RecycleStage();

}

Server::~Server()
{
    delete read_stage_;
    delete out_stage_;
    delete recycle_stage_;

    shutdown(fd_, SHUT_RDWR);
    close(fd_);
}

void
Server::initialize_stages()
{
    read_stage_->initialize();
    out_stage_->initialize();
    recycle_stage_->initialize();

    recycle_stage_->start_thread();
    for (int i = 0; i < 1; i++) {
        read_stage_->start_thread();
        out_stage_->start_thread();
    }
}

void
Server::listen(int queue_size)
{
    if (::listen(fd_, queue_size) < 0)
        throw SyscallException();
}

void
Server::main_loop()
{
    ::signal(SIGPIPE, SIG_IGN);
    Pipeline& pipeline = Pipeline::instance();
    Stage* stage = pipeline.find_stage("poll_in");
    while (true) {
        InternetAddress address;
        socklen_t socklen = address.max_address_length();
        int client_fd = ::accept(fd_, address.get_address(), &socklen);
        if (client_fd < 0) {
            LOG(WARNING, "Error when accepting socket: %s", strerror(errno));
            continue;
        }
        // set non-blocking mode
        Connection* conn = pipeline.create_connection();
        conn->address = address;
        conn->fd = client_fd;
        conn->prio = 0;
        conn->inactive = false;
        utils::set_socket_blocking(conn->fd, false);

        LOG(INFO, "accepted connection from %s",
            conn->address_string().c_str());
        stage->sched_add(conn);
    }
}

}
