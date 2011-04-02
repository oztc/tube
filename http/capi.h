#ifndef _CAPI_H_
#define _CAPI_H_

#include <stdint.h>
#include <sys/types.h>

__BEGIN_DECLS

// only for LP/LLP64 model
typedef unsigned char          u8;
typedef unsigned short int     u16;
typedef unsigned int           u32;
typedef unsigned long long int u64;

typedef char          int8;
typedef short int     int16;
typedef int           int23;
typedef long long int int64;

typedef void tube_http_request_t;
typedef void tube_http_response_t;

typedef struct _tube_http_handler_t tube_http_handler_t;
typedef struct _tube_http_handler_desc_t tube_http_handler_desc_t;

struct _tube_http_handler_t {
    void (*handle_request) (tube_http_handler_t* handler,
                            tube_http_request_t* request,
                            tube_http_response_t* response);
    void (*load_param) (tube_http_handler_t* handler);

    void* handle;
};

struct _tube_http_handler_desc_t {
    tube_http_handler_t* (*create_handler) ();
    const char* module_name;
    const char* vender_name;
};

#define MAX_OPTION_LEN 128
#define MAX_HEADER_VALUE_LEN 128

const char*  tube_http_handler_get_name(tube_http_handler_t* handler);
void         tube_http_handler_option(tube_http_handler_t* handler,
                                      const char* name,
                                      char* value);
void         tube_http_handler_add_option(tube_http_handler_t* handler,
                                          const char* name,
                                          const char* value);

void         tube_http_handler_desc_register(tube_http_handler_desc_t* desc);

short        tube_http_request_get_method(tube_http_request_t* request);
u64          tube_http_request_get_content_length(tube_http_request_t* request);
short        tube_http_request_get_transfer_encoding(
    tube_http_request_t* request);

short        tube_http_request_get_version_major(tube_http_request_t* request);
short        tube_http_request_get_version_minor(tube_http_request_t* request);
int          tube_http_request_get_keep_alive(tube_http_request_t* request);
const char*  tube_http_request_get_path(tube_http_request_t* request);
const char*  tube_http_request_get_uri(tube_http_request_t* request);
const char*  tube_http_request_get_query_string(tube_http_request_t* request);
const char*  tube_http_request_get_fragment(tube_http_request_t* request);
const char*  tube_http_request_get_method_string(tube_http_request_t* request);
void         tube_http_request_set_uri(tube_http_request_t* request,
                                       const char* uri);
const char*  tube_http_request_find_header_value(tube_http_request_t* request,
                                                 const char* key);
int          tube_http_request_has_header(tube_http_request_t* request,
                                          const char* key);

size_t       tube_http_request_find_header_values(
    tube_http_request_t* request, const char* key,
    char res[][MAX_HEADER_VALUE_LEN], size_t max_value_num);

ssize_t      tube_http_request_read_data(tube_http_request_t* request,
                                         void* ptr, size_t size);

/* response api */
void         tube_http_response_add_header(tube_http_response_t* response,
                                           const char* key, const char* value);
void         tube_http_response_set_has_content_length(
    tube_http_response_t* response, int val);
int          tube_http_response_has_content_length(
    tube_http_response_t* response);
void         tube_http_response_set_content_length(
    tube_http_response_t* response, u64 length);

u64          tube_http_response_get_content_length(
    tube_http_response_t* response);
void         tube_http_response_disable_prepare_buffer(
    tube_http_response_t* response);

ssize_t      tube_http_response_write_data(tube_http_response_t* response,
                                           const void* ptr, size_t size);
ssize_t      tube_http_response_write_string(tube_http_response_t* response,
                                             const char* str);
void         tube_http_response_write_file(tube_http_response_t* response,
                                           int file_desc,
                                           off64_t offset, off64_t length);
void         tube_http_response_flush_data(tube_http_response_t* response);
void         tube_http_response_close(tube_http_response_t* response);
void         tube_http_response_respond(tube_http_response_t* response,
                                        int status_code,
                                        const char* reason);
__END_DECLS

#endif /* _CAPI_H_ */
