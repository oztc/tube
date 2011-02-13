#include "core/stages.h"
#include "core/wrapper.h"

namespace pipeserv {

Wrapper::Wrapper(Connection* conn)
    : conn_(conn), pipeline_(Pipeline::instance())
{}

Request::Request(Connection* conn)
  : Wrapper(conn)
{}

Request::~Request()
{}

ssize_t
Request::read_data(byte* ptr, size_t sz)
{
    Buffer& buf = conn_->in_buf;
    ssize_t ret = 0;
    if (buf.size() >= sz) {
        memcpy((void*) ptr, buf.ptr(), sz);
        buf.pop(sz);
        return sz;
    }

    if (buf.size() > 0) {
        memcpy((void*) ptr, buf.ptr(), buf.size());
        ptr += buf.size();
        sz -= buf.size();
        buf.clear();
    }
    disable_poll();
    ret = ::read(conn_->fd, (void*) ptr, sz);
    enable_poll();
    return ret;
}

Response::Response(Connection* conn, size_t buffer_size)
    : Wrapper(conn), buffer_size_(buffer_size), inactive_(false)
{
    poll_out_stage_ = Pipeline::instance().find_stage("poll_out");
}

Response::~Response()
{
    if (active()) {
        conn_->hold = true; // hold the lock, so conn_->unlock won't unlock
        poll_out_stage_->sched_add(conn_); // silently flush
    }
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
    while (buf.size() > 0) {
        ssize_t rs = ::write(conn_->fd, buf.ptr(), buf.size());
        if (rs <= 0) {
            return rs;
        }
        buf.pop(rs);
    }
    return ret;
}

void
Response::close()
{
    inactive_ = true;
    ::close(conn_->fd); // this will trigger EPOLLHUP event
}

}
