#include "core/stages.h"
#include "core/wrapper.h"

namespace pipeserv {

Wrapper::Wrapper(Connection* conn)
    : conn_(conn), pipeline_(Pipeline::instance())
{}

Request::Request(Connection* conn)
  : Wrapper(conn)
{
    utils::set_socket_blocking(conn->fd, true);
}

Request::~Request()
{
    utils::set_socket_blocking(conn_->fd, false);
}

ssize_t
Request::read_data(byte* ptr, size_t sz)
{
    Buffer& buf = conn_->in_buf;
    if (buf.size() > 0) {
        size_t len = 0;
        byte* bufptr = buf.get_page_segment(buf.first_page(), &len);
        memcpy(ptr, bufptr, len);
        return buf.pop_page();
    } else {
        disable_poll();
        ssize_t nread = ::read(conn_->fd, (void*) ptr, sz);
        enable_poll();
        return nread;
    }
}

Response::Response(Connection* conn, size_t buffer_size)
    : Wrapper(conn), buffer_size_(buffer_size), inactive_(false)
{
    poll_out_stage_ = Pipeline::instance().find_stage("poll_out");
}

Response::~Response()
{
    if (response_code() < 0) {
        poll_out_stage_->sched_add(conn_); // silently flush
    }
}

int
Response::response_code() const
{
    if (active() && conn_->out_buf.size() > 0)
        return -1;
    return 0;
}

ssize_t
Response::write_data(const byte* ptr, size_t sz)
{
    Buffer& buf = conn_->out_buf;
    ssize_t ret;
    buf.append(ptr, sz);

    if (buf.size() > buffer_size_) {
        ret = flush_data();
        if (ret <= 0) return ret;
    }
    return sz;
}

ssize_t
Response::flush_data()
{
    Buffer& buf = conn_->out_buf;
    ssize_t ret = buf.size();
    disable_poll();
    while (buf.size() > 0) {
        ssize_t rs = buf.write_to_fd(conn_->fd);
        if (rs <= 0) {
            return rs;
        }
        buf.pop(rs);
    }
    enable_poll();
    return ret;
}

void
Response::close()
{
    inactive_ = true;
    ::close(conn_->fd); // this will trigger EPOLLHUP event
}

}
