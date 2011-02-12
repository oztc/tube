#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <cstdlib>

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
}

Server::~Server()
{
    shutdown(fd_, SHUT_RDWR);
    close(fd_);
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
    Pipeline& pipeline = Pipeline::instance();
    Stage* stage = pipeline.find_stage("poll_in");
    while (true) {
        struct sockaddr* addr = (struct sockaddr*) malloc(addr_size_);
        socklen_t len = addr_size_;
        memset(addr, 0, addr_size_);
        LOG(INFO, "accepting...");
        int client_fd = ::accept(fd_, addr, &len);
        if (client_fd < 0) {
            free(addr);
            LOG(WARNING, "Error when accepting socket: %s", strerror(errno));
            continue;
        }
        // set non-blocking mode
        Connection* conn = pipeline.create_connection();
        conn->addr = addr;
        conn->addr_len = len;
        conn->fd = client_fd;
        conn->prio = 0;
        conn->inactive = false;
        LOG(DEBUG, "changing into blocked mode");
        utils::set_socket_blocking(conn->fd, false);

        LOG(INFO, "accepted one connection");
        stage->sched_add(conn);
    }
}

}
