#include "pch.h"

#include "http/http_wrapper.h"
#include "http/http_parser.h"

namespace pipeserv {

// some common http response status defined by the standard
// copied from http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html

const HttpResponseStatus
HttpResponseStatus::kHttpResponseContinue =
    HttpResponseStatus(100, "Continue");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseSwitchingProtocols =
    HttpResponseStatus(101, "Switching Protocols");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseOK =
    HttpResponseStatus(200, "OK");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseCreated =
    HttpResponseStatus(201, "Created");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseAccepted =
    HttpResponseStatus(202, "Accepted");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseNonAuthoritativeInformation =
    HttpResponseStatus(203, "Non-Authoritative Information");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseNoContent =
    HttpResponseStatus(204, "No Content");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseResetContent =
    HttpResponseStatus(205, "Reset Content");

const HttpResponseStatus
HttpResponseStatus::kHttpResponsePartialContent =
    HttpResponseStatus(206, "Partial Content");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseMultipleChoices =
    HttpResponseStatus(300, "Multiple Choices");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseMovedPermanently =
    HttpResponseStatus(301, "Moved Permanently");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseFound =
    HttpResponseStatus(302, "Found");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseSeeOther =
    HttpResponseStatus(303, "See Other");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseNotModified =
    HttpResponseStatus(304, "Not Modified");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseUseProxy =
    HttpResponseStatus(305, "Use Proxy");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseTemporaryRedirect =
    HttpResponseStatus(307, "Temporary Redirect");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseBadRequest =
    HttpResponseStatus(400, "Bad Request");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseUnauthorized =
    HttpResponseStatus(401, "Unauthorized");

const HttpResponseStatus
HttpResponseStatus::kHttpResponsePaymentRequired =
    HttpResponseStatus(402, "Payment Required");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseForbidden =
    HttpResponseStatus(403, "Forbidden");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseNotFound =
    HttpResponseStatus(404, "Not Found");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseMethodNotAllowed =
    HttpResponseStatus(405, "Method Not Allowed");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseNotAcceptable =
    HttpResponseStatus(406, "Not Acceptable");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseProxyAuthenticationRequired =
    HttpResponseStatus(407, "Proxy Authentication Required");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseRequestTimeout =
    HttpResponseStatus(408, "Request Time-out");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseConflict =
    HttpResponseStatus(409, "Conflict");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseGone =
    HttpResponseStatus(410, "Gone");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseLengthRequired =
    HttpResponseStatus(411, "Length Required");

const HttpResponseStatus
HttpResponseStatus::kHttpResponsePreconditionFailed =
    HttpResponseStatus(412, "Precondition Failed");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseRequestEntityTooLarge =
    HttpResponseStatus(413, "Request Entity Too Large");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseRequestUriTooLarge =
    HttpResponseStatus(414, "Request-URI Too Large");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseUnsupportedMediaType =
    HttpResponseStatus(415, "Unsupported Media Type");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseRequestedrangenotsatisfiable =
    HttpResponseStatus(416, "Requested range not satisfiable");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseExpectationFailed =
    HttpResponseStatus(417, "Expectation Failed");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseInternalServerError =
    HttpResponseStatus(500, "Internal Server Error");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseNotImplemented =
    HttpResponseStatus(501, "Not Implemented");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseBadGateway =
    HttpResponseStatus(502, "Bad Gateway");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseServiceUnavailable =
    HttpResponseStatus(503, "Service Unavailable");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseGatewayTimeout =
    HttpResponseStatus(504, "Gateway Time-out");

const HttpResponseStatus
HttpResponseStatus::kHttpResponseHttpVersionNotSupported =
    HttpResponseStatus(505, "HTTP Version not supported");

// end of common http response status definition

HttpResponseStatus::HttpResponseStatus(int code, std::string reason_string)
    : status_code(code), reason(reason_string)
{}

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
    default:
        return std::string();
    }
}

bool
HttpRequest::has_header(std::string key) const
{
    const HttpHeaderEnumerate& headers = request_.headers;
    for (size_t i = 0; i < headers.size(); i++) {
        if (headers[i].key == key)
            return true;
    }
    return false;
}

std::vector<std::string>
HttpRequest::find_header_values(std::string key) const
{
    const HttpHeaderEnumerate& headers = request_.headers;
    std::vector<std::string> result;
    for (size_t i = 0; i < headers.size(); i++) {
        if (headers[i].key == key)
            result.push_back(headers[i].value);
    }
    return result;
}

std::string
HttpRequest::find_header_value(std::string key) const
{
    const HttpHeaderEnumerate& headers = request_.headers;
    for (size_t i = 0; i < headers.size(); i++) {
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
    for (size_t i = 0; i < p.length(); i++) {
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
    for (size_t i = 0; i < headers_.size(); i++) {
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
    is_responded_ = true;
}

void
HttpResponse::reset()
{
    prepare_buffer_ = Buffer(); // create a new empty buffer;
    content_length_ = -1;
    headers_.clear();
    has_content_length_ = true;
    is_responded_ = false;
}

// decode and encode url
static int8
char_to_hex(char ch)
{
    static const char* hex_char = "0123456789ABCDEF";
    const char* ptr = strchr(hex_char, ch);
    if (ptr == NULL)
        return -1;
    else
        return ptr - hex_char;
}

static std::string
http_url_decode(const std::string& url)
{
    std::string res;
    for (size_t i = 0; i < url.length(); i++) {
        int8 p = 0, q = 0;
        unsigned char ch = 0;
        if (url[i] != '%')
            goto pass;
        if (i + 2 >= url.length())
            goto pass;
        p = char_to_hex(url[i + 1]);
        q = char_to_hex(url[i + 2]);
        if (p == -1 || q == -1)
            goto pass;
        ch = (p << 4) | q;
        res += ch;
        i += 2;
        continue;
    pass:
        res += url[i];
    }
    return res;
}

std::string
HttpRequest::url_decode(std::string url)
{
    return http_url_decode(url);
}

}

