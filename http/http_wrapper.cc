#include <sstream>

#include "http/http_wrapper.h"
#include "http/http_parser.h"

namespace pipeserv {

HttpRequest::HttpRequest(Connection* conn, const HttpRequestData& request)
    : Request(conn), request_(request)
{
}

std::string
HttpRequest::method_string() const
{
    switch (method()) {
    case HTTP_COPY :
        return std::string("COPY");
    case HTTP_DELETE:
        return std::string("DELETE");
    case HTTP_GET:
        return std::string("GET");
    case HTTP_HEAD:
        return std::string("HEAD");
    case HTTP_LOCK:
        return std::string("LOCK");
    case HTTP_MKCOL:
        return std::string("MKCOL");
    case HTTP_MOVE:
        return std::string("MOVE");
    case HTTP_OPTIONS:
        return std::string("OPTIONS");
    case HTTP_POST:
        return std::string("POST");
    case HTTP_PROPFIND:
        return std::string("PROPFIND");
    case HTTP_PROPPATCH:
        return std::string("PROPFIND");
    case HTTP_PUT:
        return std::string("PUT");
    case HTTP_TRACE:
        return std::string("TRACE");
    case HTTP_UNLOCK:
        return std::string("UNLOCK");
    }
}

std::vector<std::string>
HttpRequest::find_header_values(std::string key) const
{
    const HttpHeaderEnumerate& headers = request_.headers;
    std::vector<std::string> result;
    for (int i = 0; i < headers.size(); i++) {
        if (headers[i].key == key)
            result.push_back(headers[i].value);
    }
    return result;
}

std::string
HttpRequest::find_header_value(std::string key) const
{
    const HttpHeaderEnumerate& headers = request_.headers;
    for (int i = 0; i < headers.size(); i++) {
        if (headers[i].key == key)
            return headers[i].value;
    }
    return "";
}

static bool
ignore_compare(const std::string& p, const std::string& q)
{
    if (p.length() != q.length())
        return false;
    for (int i = 0; i < p.length(); i++) {
        if (p[i] != q[i] && abs(p[i] - q[i]) != 'a' - 'A')
            return false;
    }
    return true;
}

const char* HttpResponse::kHttpVersion = "HTTP/1.1";
const char* HttpResponse::kHttpNewLine = "\r\n";

HttpResponse::HttpResponse(Connection* conn) : Response(conn)
{
    reset();
}

void
HttpResponse::add_header(std::string key, std::string value)
{
    if (ignore_compare(key, std::string("content-length"))) {
        content_length_ = atoll(value.c_str());
    } else {
        headers_.push_back(HttpHeaderItem(key, value));
    }
}

ssize_t
HttpResponse::write_data(const byte* ptr, size_t size)
{
    prepare_buffer_.append(ptr, size);
    return size;
}

void
HttpResponse::respond(const HttpResponseStatus& status)
{
    // construct the header and send it long with the prepare buffer
    if (content_length_ < 0)
        set_content_length(prepare_buffer_.size());

    std::stringstream response_text;
    response_text << kHttpVersion << " " << status.status_code << " "
                  << status.reason << kHttpNewLine;
    for (int i = 0; i < headers_.size(); i++) {
        const HttpHeaderItem& item = headers_[i];
        response_text << item.key << ": " << item.value << kHttpNewLine;
    }
    if (has_content_length_) {
        response_text << "Content-Length: " << content_length_ << kHttpNewLine;
    }
    response_text << kHttpNewLine;
    conn_->out_stream.append_data((const byte*) response_text.str().c_str(),
                                  response_text.str().length());
    if (prepare_buffer_.size() > 0) {
        // send the body if have any
        conn_->out_stream.append_buffer(prepare_buffer_);
    }
    reset();
}

void
HttpResponse::reset()
{
    prepare_buffer_ = Buffer(); // create a new empty buffer;
    content_length_ = -1;
    headers_.clear();
    has_content_length_ = true;
}

}

