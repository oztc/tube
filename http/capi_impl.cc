#include "pch.h"

#include "http/capi.h"
#include "http/interface.h"
#include "http/configuration.h"

typedef pipeserv::BaseHttpHandler* http_handler_handle_t;

#define HTTP_REQUEST(ptr) ((pipeserv::HttpRequest*) (ptr))
#define HTTP_RESPONSE(ptr) ((pipeserv::HttpResponse*) (ptr))
#define HANDLER_IMPL(ptr) ((pipeserv::CHttpHandlerAdapter*) (ptr))

namespace pipeserv {

class CHttpHandlerAdapter : public BaseHttpHandler
{
    http_handler_t* handler_;
public:
    CHttpHandlerAdapter(http_handler_t* handler) : handler_(handler) {
        handler_->handle = this;
    }

    virtual ~CHttpHandlerAdapter() {}

    virtual void handle_request(HttpRequest& request, HttpResponse& response) {
        handler_->handle_request(handler_, &request, &response);
    }

    virtual void load_param() {
        handler_->load_param(handler_);
    }
};

class CHttpHandlerFactoryAdapter : public BaseHttpHandlerFactory
{
    http_handler_descriptor_t* desc_;
public:
    CHttpHandlerFactoryAdapter(http_handler_descriptor_t* desc) : desc_(desc) {}

    virtual BaseHttpHandler* create() const {
        return new CHttpHandlerAdapter(desc_->create_handler());
    }

    virtual std::string module_name() const {
        return desc_->module_name;
    }

    virtual std::string vender_name() const {
        return desc_->vender_name;
    }
};

}

#define EXPORT_API extern "C"

EXPORT_API const char*
http_handler_get_name(http_handler_t* handler)
{
    return HANDLER_IMPL(handler->handle)->name().c_str();
}

EXPORT_API void
http_handler_option(http_handler_t* handler, const char* name, char* value)
{
    std::string str = HANDLER_IMPL(handler->handle)->option(name);
    strncpy(value, str.c_str(), MAX_OPTION_LEN);
}

EXPORT_API void
http_handler_add_option(http_handler_t* handler, const char* name,
                        const char* value)
{
    HANDLER_IMPL(handler->handle)->add_option(name, value);
}

EXPORT_API void
http_handler_descriptor_register(http_handler_descriptor_t* desc)
{
    pipeserv::BaseHttpHandlerFactory::register_factory(
        new pipeserv::CHttpHandlerFactoryAdapter(desc));
}

// request api
#define REQ_DATA(request) ((request)->request_data_ref())

#define DEF_PRIMITIVE(type, name)                                       \
    extern "C" type http_request_get_##name(http_request_t* req) {    \
        return HTTP_REQUEST(req)->name(); }                             \

#define DEF_DATA(type, name, mem)                                       \
    extern "C" type http_request_get_##name(http_request_t* req) {    \
        return REQ_DATA(HTTP_REQUEST(req)).mem; }                     \

DEF_PRIMITIVE(short, method);
DEF_PRIMITIVE(u64, content_length);
DEF_PRIMITIVE(short, transfer_encoding);
DEF_PRIMITIVE(short, version_major);
DEF_PRIMITIVE(short, version_minor);
DEF_PRIMITIVE(int, keep_alive);

DEF_DATA(const char*, path, path.c_str());
DEF_DATA(const char*, uri, uri.c_str());
DEF_DATA(const char*, query_string, query_string.c_str());
DEF_DATA(const char*, fragment, fragment.c_str());
DEF_DATA(const char*, method_string, method_string());

EXPORT_API void
http_request_set_uri(http_request_t* request, const char* uri)
{
    HTTP_REQUEST(request)->set_uri(std::string(uri));
}

EXPORT_API const char*
http_request_find_header_value(http_request_t* request, const char* key)
{
    pipeserv::HttpHeaderEnumerate& headers = HTTP_REQUEST(request)->headers();
    for (size_t i = 0; i < headers.size(); i++) {
        if (headers[i].key == key) {
            return headers[i].value.c_str();
        }
    }
    return NULL;
}

EXPORT_API int
http_request_has_header(http_request_t* request, const char* key)
{
    return http_request_find_header_value(request, key) != NULL;
}


EXPORT_API size_t
http_request_find_header_values(http_request_t* request, const char* key,
                                char res[][MAX_HEADER_VALUE_LEN],
                                size_t max_value_num)
{
    std::vector<std::string> values =
        HTTP_REQUEST(request)->find_header_values(key);
    size_t i;
    for (i = 0; i < max_value_num && i < values.size(); i++) {
        memset(res[i], 0, MAX_HEADER_VALUE_LEN);
        strncpy(res[i], values[i].c_str(), MAX_HEADER_VALUE_LEN);
    }
    return i;
}

EXPORT_API ssize_t
http_request_read_data(http_request_t* request, void* ptr, size_t size)
{
    return HTTP_REQUEST(request)->read_data((pipeserv::byte*) ptr, size);
}

// response api
EXPORT_API void
http_response_add_header(http_response_t* response, const char* key,
                         const char* value)
{
    HTTP_RESPONSE(response)->add_header(key, value);
}

EXPORT_API void
http_response_set_has_content_length(http_response_t* response, int val)
{
    HTTP_RESPONSE(response)->set_has_content_length(val);
}

EXPORT_API int
http_response_has_content_length(http_response_t* response)
{
    return HTTP_RESPONSE(response)->has_content_length();
}

EXPORT_API void
http_response_set_content_length(http_response_t* response, u64 length)
{
    HTTP_RESPONSE(response)->set_content_length(length);
}

EXPORT_API u64
http_response_get_content_length(http_response_t* response)
{
    return HTTP_RESPONSE(response)->content_length();
}

EXPORT_API void
http_response_disable_prepare_buffer(http_response_t* response)
{
    HTTP_RESPONSE(response)->disable_prepare_buffer();
}

EXPORT_API ssize_t
http_response_write_data(http_response_t* response, const void* ptr,
                         size_t size)
{
    return HTTP_RESPONSE(response)->write_data((const pipeserv::byte*) ptr,
                                               size);
}

EXPORT_API ssize_t
http_response_write_string(http_response_t* response, const char* str)
{
    return HTTP_RESPONSE(response)->write_string(str);
}

EXPORT_API void
http_response_write_file(http_response_t* response, int file_desc,
                         off64_t offset, off64_t length)
{
    HTTP_RESPONSE(response)->write_file(file_desc, offset, length);
}

EXPORT_API void
http_response_flush_data(http_response_t* response)
{
    HTTP_RESPONSE(response)->flush_data();
}

EXPORT_API void
http_response_close(http_response_t* response)
{
    HTTP_RESPONSE(response)->close();
}

EXPORT_API void
http_response_respond(http_response_t* response, int status_code,
                      const char* reason)
{
    HTTP_RESPONSE(response)->respond(pipeserv::HttpResponseStatus(status_code,
                                                                  reason));
}
