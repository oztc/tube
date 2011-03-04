#include <cstdio>
#include <iostream>
#include <ctime>

#include "http/connection.h"
#include "utils/logger.h"

using namespace pipeserv;

static const char* protocol_text = "GET / HTTP/1.1\r\nHost: www.test.com\r\n\r\nGET / HTTP/1.1\r\nContent-Length: 13\r\n\r\n1234567890abcGET / HTTP/1.1\r\nHost: www.test.com\r\n\r\n";

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static void
process_stub(HttpConnection* conn)
{
    Buffer& buf = conn->in_stream.buffer();
    if (conn->is_ready()) {
        u64 clen = conn->get_request_data_list().back().content_length;
        fprintf(stderr, "consume the content with size %d\n", clen);
        buf.pop(clen);
        conn->get_request_data_list().clear();
    }
}

static void
random_test(HttpConnection* conn)
{
    Buffer& buf = conn->in_stream.buffer();
    const char* ptr = protocol_text;
    size_t len = strlen(protocol_text);
    while (len > 0) {
        size_t step = rand() % MIN(len, 10) + 1;
        buf.append((const byte*) ptr, step);
        if (!conn->do_parse()) {
            fprintf(stderr, "Error\n");
            break;
        }
        process_stub(conn);
        ptr += step;
        len -= step;
    }
}

static void
continous_test(HttpConnection* conn)
{
    Buffer& buf = conn->in_stream.buffer();
    const char* ptr = protocol_text;
    size_t len = strlen(protocol_text);
    buf.append((const byte*) ptr, len);
    while (buf.size() > 0) {
        if (!conn->do_parse()) {
            fprintf(stderr, "Error\n");
            break;
        }
        process_stub(conn);
    }
}

int
main(int argc, char *argv[])
{
    utils::logger.set_level(DEBUG);
    srand(time(NULL));
    HttpConnection* conn = new HttpConnection(0);
    random_test(conn);
    continous_test(conn);
    delete conn;
    return 0;
}

