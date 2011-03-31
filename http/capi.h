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

typedef void* handle_t;
typedef void http_request_t;
typedef void http_response_t;

typedef struct _http_handler_t http_handler_t;
typedef struct _http_handler_descriptor_t http_handler_descriptor_t;

struct _http_handler_t {
    void (*handle_request) (http_handler_t* handler,
                            http_request_t* request,
                            http_response_t* response);
    void (*load_param) (http_handler_t* handler);

    handle_t handle;
};

struct _http_handler_descriptor_t {
    http_handler_t* (*create_handler) ();
    const char* module_name;
    const char* vender_name;
};

#define MAX_OPTION_LEN 128
#define MAX_HEADER_VALUE_LEN 128

const char*  http_handler_get_name(http_handler_t* handler);
void         http_handler_option(http_handler_t* handler, const char* name,
                                 char* value);
void         http_handler_add_option(http_handler_t* handler, const char* name,
                                     const char* value);
void         http_handler_descriptor_register(http_handler_descriptor_t* desc);

short        http_request_get_method(http_request_t* request);
u64          http_request_get_content_length(http_request_t* request);
short        http_request_get_transfer_encoding(http_request_t* request);
short        http_request_get_version_major(http_request_t* request);
short        http_request_get_version_minor(http_request_t* request);
int          http_request_get_keep_alive(http_request_t* request);
const char*  http_request_get_path(http_request_t* request);
const char*  http_request_get_uri(http_request_t* request);
const char*  http_request_get_query_string(http_request_t* request);
const char*  http_request_get_fragment(http_request_t* request);
const char*  http_request_get_method_string(http_request_t* request);
void         http_request_set_uri(http_request_t* request, const char* uri);
const char*  http_request_find_header_value(http_request_t* request,
                                            const char* key);
int          http_request_has_header(http_request_t* request,
                                     const char* key);
size_t       http_request_find_header_values(http_request_t* request,
                                             const char* key,
                                             char res[][MAX_HEADER_VALUE_LEN],
                                             size_t max_value_num);
ssize_t      http_request_read_data(http_request_t* request, void* ptr,
                                    size_t size);

/* response api */
void         http_response_add_header(http_response_t* response,
                                      const char* key, const char* value);
void         http_response_set_has_content_length(http_response_t* response,
                                                  int val);
int          http_response_has_content_length(http_response_t* response);
void         http_response_set_content_length(http_response_t* response,
                                              u64 length);
u64          http_response_get_content_length(http_response_t* response);
void         http_response_disable_prepare_buffer(http_response_t* response);
ssize_t      http_response_write_data(http_response_t* response,
                                      const void* ptr, size_t size);
ssize_t      http_response_write_string(http_response_t* response,
                                        const char* str);
void         http_response_write_file(http_response_t* response, int file_desc,
                                      off64_t offset, off64_t length);
void         http_response_flush_data(http_response_t* response);
void         http_response_close(http_response_t* response);
void         http_response_respond(http_response_t* response, int status_code,
                                   const char* reason);
__END_DECLS

#endif /* _CAPI_H_ */
