#include <cstdio>

#include "core/stages.h"
#include "core/wrapper.h"

namespace pipeserv {

Wrapper::Wrapper(Connection* conn)
    : conn_(conn), pipeline_(Pipeline::instance())
{}

Request::Request(Connection* conn)
  : Wrapper(conn)
{
    disable_poll();
}

Request::~Request()
{
    enable_poll();
}

ssize_t
Request::read_data(byte* ptr, size_t sz)
{
    Buffer& buf = conn_->in_stream.buffer();
    if (buf.size() > 0) {
        size_t len = 0;
        byte* bufptr = buf.get_page_segment(buf.first_page(), &len);
        memcpy(ptr, bufptr, len);
        return buf.pop_page();
    } else {
        utils::set_socket_blocking(conn_->fd, true);
        ssize_t nread = ::read(conn_->fd, (void*) ptr, sz);
        utils::set_socket_blocking(conn_->fd, false);
        return nread;
    }
}

size_t Response::kMaxMemorySizes = (4 << 20);

Response::Response(Connection* conn)
    : Wrapper(conn), max_mem_(kMaxMemorySizes), inactive_(false)
{
    out_stage_ = Pipeline::instance().find_stage("write_back");
}

Response::~Response()
{
    if (response_code() < 0) {
        out_stage_->sched_add(conn_); // silently flush
    }
}

int
Response::response_code() const
{
    if (active() && !conn_->out_stream.is_done())
        return -1;
    return 0;
}

ssize_t
Response::write_data(const byte* ptr, size_t sz)
{
    OutputStream& out = conn_->out_stream;
    ssize_t ret;
    out.append_data(ptr, sz);

    if (out.memory_usage() > max_mem_) {
        ret = flush_data();
        if (ret <= 0) return ret;
    }
    return sz;
}

void
Response::write_file(std::string filename, off64_t offset, off64_t length)
{
    conn_->out_stream.append_file(filename, offset, length);
}

ssize_t
Response::flush_data()
{
    OutputStream& out = conn_->out_stream;
    ssize_t nwrite = 0;
    disable_poll();
    utils::set_socket_blocking(conn_->fd, true);
    while (true) {
        ssize_t rs = out.write_into_output();
        if (rs < 0) {
            nwrite = rs;
            break;
        } else if (rs == 0) {
            break;
        }
        nwrite += rs;
    }
    utils::set_socket_blocking(conn_->fd, false);
    enable_poll();
    return nwrite;
}

void
Response::close()
{
    inactive_ = true;
    conn_->active_close();
}

}
