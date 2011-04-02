#include "pch.h"

#include "http/connection.h"
#include "http/configuration.h"
#include "utils/logger.h"

namespace tube {

#define HTTP_CONNECTION(ptr) ((HttpConnection*) (ptr))

#define DEF_CALLBACK_PROC(name, method)                                 \
    static int name(http_parser* parser, const char* at, size_t length) \
    {                                                                   \
        HTTP_CONNECTION(parser->data)->method(at, length);              \
        return 0;                                                       \
    }                                                                   \

DEF_CALLBACK_PROC(on_field, append_field);
DEF_CALLBACK_PROC(on_value, append_value);
DEF_CALLBACK_PROC(on_uri, append_uri);
DEF_CALLBACK_PROC(on_query_string ,append_query_string);
DEF_CALLBACK_PROC(on_fragment, append_fragment);
DEF_CALLBACK_PROC(on_path, append_path);
DEF_CALLBACK_PROC(on_chunk_data, append_chunk);

static int
on_message_complete(http_parser* parser)
{
    HTTP_CONNECTION(parser->data)->finish_parse();
    return 0;
}

static int
on_header_line_complete(http_parser* parser)
{
    HTTP_CONNECTION(parser->data)->finish_header_line();
    return 0;
}

HttpRequestData::HttpRequestData()
    : method(0), content_length(0), transfer_encoding(0), version_major(0),
      version_minor(0), keep_alive(false)
{
}

const char*
HttpRequestData::method_string() const
{
    switch (method) {
    case HTTP_COPY :
        return "COPY";
    case HTTP_DELETE:
        return "DELETE";
    case HTTP_GET:
        return "GET";
    case HTTP_HEAD:
        return "HEAD";
    case HTTP_LOCK:
        return "LOCK";
    case HTTP_MKCOL:
        return "MKCOL";
    case HTTP_MOVE:
        return "MOVE";
    case HTTP_OPTIONS:
        return "OPTIONS";
    case HTTP_POST:
        return "POST";
    case HTTP_PROPFIND:
        return "PROPFIND";
    case HTTP_PROPPATCH:
        return "PROPFIND";
    case HTTP_PUT:
        return "PUT";
    case HTTP_TRACE:
        return "TRACE";
    case HTTP_UNLOCK:
        return "UNLOCK";
    default:
        return "";
    }
}

HttpConnection::HttpConnection(int fd)
    : Connection(fd)
{
    http_parser_init(&parser_, HTTP_REQUEST);
    parser_.data = this;
    parser_.on_message_complete = on_message_complete;
    parser_.on_header_line_complete = on_header_line_complete;
    parser_.on_header_field = on_field;
    parser_.on_header_value = on_value;
    parser_.on_path = on_path;
    parser_.on_uri = on_uri;
    parser_.on_query_string = on_query_string;
    parser_.on_fragment = on_fragment;
    parser_.on_chunk_data = on_chunk_data;

    set_io_timeout(500); // max block time
}

const size_t HttpConnection::kMaxBodySize = 16 << 10;

bool
HttpConnection::do_parse()
{
    if (!requests_.empty() && requests_.back().content_length > 0) {
        // here, we don't consume the buffer, we'll leave the buffer to the
        // application to consume it, since it's a stream
        return true;
    }
    Buffer& buf = in_stream.buffer();
    size_t nconsumed = 0;
    for (Buffer::PageIterator it = buf.page_begin(); it != buf.page_end();
         ++it) {
        size_t len = 0;
        const char* ptr = (const char*) buf.get_page_segment(*it, &len);
        //LOG(DEBUG, "parsing %.*s", len, ptr);
        nconsumed += http_parser_execute(&parser_, ptr, len);
        if (!requests_.empty() && requests_.back().content_length > 0) {
            break;
        }
        if (http_parser_has_error(&parser_)) {
            LOG(WARNING, "error when parsing http packet at %*s", (int) len,
                ptr);
            break;
        }
    }
    buf.pop(nconsumed);
    return !http_parser_has_error(&parser_);
}

bool
HttpConnection::has_error() const
{
    return http_parser_has_error(&parser_);
}

bool
HttpConnection::is_ready() const
{
    if (requests_.empty())
        return false;
    const HttpRequestData& req_data = requests_.back();
    if (req_data.content_length < kMaxBodySize
        && in_stream.buffer().size() < req_data.content_length) {
        // content-length is small enought, we should wait until buffer
        // exceed the content-length
        return false;
    }
    return true;
}

void
HttpConnection::append_field(const char* ptr, size_t sz)
{
    last_header_key_.append(ptr, sz);
}

void
HttpConnection::append_value(const char* ptr, size_t sz)
{
    last_header_value_.append(ptr, sz);
}

void
HttpConnection::append_uri(const char* ptr, size_t sz)
{
    tmp_request_.uri.append(ptr, sz);
}

void
HttpConnection::append_path(const char* ptr, size_t sz)
{
    tmp_request_.path.append(ptr, sz);
}

void
HttpConnection::append_query_string(const char* ptr, size_t sz)
{
    tmp_request_.query_string.append(ptr, sz);
}

void
HttpConnection::append_fragment(const char* ptr, size_t sz)
{
    tmp_request_.fragment.append(ptr, sz);
}

void
HttpConnection::append_chunk(const char* ptr, size_t sz)
{
    tmp_request_.chunk_buffer.append((const byte*) ptr, sz);
}

void
HttpConnection::finish_header_line()
{
    tmp_request_.headers.push_back(
        HttpHeaderItem(last_header_key_, last_header_value_));
    last_header_key_ = "";
    last_header_value_ = "";
}

void
HttpConnection::finish_parse()
{
    VHostConfig& vhost_cfg = VHostConfig::instance();
    tmp_request_.method = parser_.method;
    tmp_request_.content_length = parser_.content_length;
    tmp_request_.transfer_encoding = parser_.transfer_encoding;
    tmp_request_.version_major = parser_.version_major;
    tmp_request_.version_minor = parser_.version_minor;
    tmp_request_.keep_alive = http_parser_should_keep_alive(&parser_);

    LOG(DEBUG, "parsed packet with content-length: %llu\n",
        tmp_request_.content_length);
    std::string host = "default";
    for (size_t i = 0; i < tmp_request_.headers.size(); i++) {
        if (tmp_request_.headers[i].key == "Host") {
            host = tmp_request_.headers[i].value;
        }
    }
    LOG(INFO, "[%s] %s from %s",  tmp_request_.method_string(),
        tmp_request_.uri.c_str(), address_string().c_str());
    // matching the rule
    tmp_request_.url_rule = vhost_cfg.match_uri(host, tmp_request_);

    requests_.push_back(tmp_request_);
    tmp_request_ = HttpRequestData();
    last_header_key_.clear();
    last_header_value_.clear();
}

}
