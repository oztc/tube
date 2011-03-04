// -*- mode: c++ -*-

#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include "http/http_parser.h"
#include "core/pipeline.h"
#include "utils/misc.h"

namespace pipeserv {

struct HttpHeaderItem
{
    std::string key;
    std::string value;

    HttpHeaderItem(std::string k, std::string v) : key(k), value(v) {}
};

typedef std::vector<HttpHeaderItem> HttpHeaderEnumerate;

struct HttpRequestData
{
    HttpHeaderEnumerate headers;
    std::string         path;
    std::string         uri;
    std::string         query_string;
    std::string         fragment;
    Buffer              chunk_buffer;

    short method; // defined in http_parser.h
    u64   content_length;
    short transfer_encoding;
    short version_major;
    short version_minor;
    bool  keep_alive;

    HttpRequestData();
};

class HttpConnection : public Connection
{
    struct http_parser         parser_;
    std::list<HttpRequestData> requests_;

    HttpRequestData tmp_request_;
    std::string     last_header_key_;
    std::string     last_header_value_;

public:

    static const size_t kMaxBodySize;

    HttpConnection(int fd);
    virtual ~HttpConnection() {}

    void append_field(const char* ptr, size_t sz);
    void append_value(const char* ptr, size_t sz);
    void append_uri(const char* ptr, size_t sz);
    void append_path(const char* ptr, size_t sz);
    void append_query_string(const char* ptr, size_t sz);
    void append_fragment(const char* ptr, size_t sz);
    void append_chunk(const char* ptr, size_t sz);

    bool do_parse();
    void finish_header_line();
    void finish_parse();

    bool has_error() const;
    bool is_ready() const;

    std::list<HttpRequestData>& get_request_data_list() { return requests_; }
};

}

#endif /* _CONNECTION_H_ */
