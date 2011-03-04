#include "core/stream.h"
#include "core/filesender.h"

namespace pipeserv {

ssize_t
InputStream::read_into_buffer()
{
    buffer_.read_from_fd(fd_);
}

void
InputStream::close()
{
    buffer_.clear();
}

OutputStream::OutputStream(int fd)
    : fd_(fd), memory_usage_(0)
{
}

OutputStream::~OutputStream()
{
    for (std::list<Writeable*>::iterator it = writeables_.begin();
         it != writeables_.end(); ++it) {
        delete *it;
    }
    writeables_.clear();
}

ssize_t
OutputStream::write_into_output()
{
    if (writeables_.empty()) {
        return 0;
    }
    Writeable* writeable = writeables_.front();
    size_t mem_use = writeable->memory_usage();
    ssize_t res = writeable->write_to_fd(fd_);
    memory_usage_ -= mem_use - writeable->memory_usage();
    if (writeable->size() == 0) {
        writeables_.pop_front();
        delete writeable;
    }
    return res;
}

void
OutputStream::append_data(const byte* data, size_t size)
{
    if (writeables_.empty()) {
        writeables_.push_back(new Buffer());
    }
    Writeable* writeable = writeables_.back();
    if (!writeable->append(data, size)) {
        // create a buffer object and push back into the list
        Writeable* buffer = new Buffer();
        buffer->append(data, size);
        writeables_.push_back(buffer);
    }
    memory_usage_ += size;
}

off64_t
OutputStream::append_file(std::string filename, off64_t offset, off64_t length)
{
    Writeable* filesender = new FileSender(filename, offset, length);
    writeables_.push_back(filesender);
    return filesender->size();
}

size_t
OutputStream::append_buffer(const Buffer& buf)
{
    Writeable* buffer = new Buffer(buf);
    writeables_.push_back(buffer);
    return buffer->size();
}

}
