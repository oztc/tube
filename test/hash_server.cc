#include <string>
#include <iostream>

#include "utils/logger.h"
#include "core/stages.h"
#include "core/server.h"
#include "core/wrapper.h"

using namespace pipeserv;

class HashParser : public ParserStage
{
public:

    unsigned int hash(char str[]) {
        unsigned int sum = 0;
        for (int i = 0; i < 16; i++) {
            sum += str[i] << i;
        }
        return sum;
    }

    virtual int process_task(Connection* conn) {
        Buffer& buf = conn->in_buf;
        if (buf.size() < 16)
            return 0;
        char str[32];
        memset(str, 0, 32);

        Response res(conn);
        while (buf.size() >= 16 && res.active()) {
            buf.copy_front((byte*) str, 16);
            buf.pop(16);
            snprintf(str, 32, "%.16d", hash(str));
            if (res.write_data((const byte*) str, 16) < 0) {
                res.close(); // close connection
            }
        }
        return res.response_code();
    }
};

class HashServer : public Server
{
    HashParser*  parse_stage_;
public:
    HashServer() : Server("0.0.0.0", "7000") {
        utils::set_fdtable_size(8096);
        utils::logger.set_level(DEBUG);
        parse_stage_ = new HashParser();
        parse_stage_->initialize();
        initialize_stages();
    }

    void start() {
        for (int i = 0; i < 2; i++) {
            parse_stage_->start_thread();
        }
    }

    virtual ~HashServer() {
        delete parse_stage_;
    }
};

static HashServer server;

int
main(int argc, char *argv[])
{
    server.start();
    server.listen(128);

    server.main_loop();
    return 0;
}

