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
        for (int i = 0; i < 15; i++) {
            sum += str[i] << i;
        }
        return sum;
    }

    virtual int process_task(Connection* conn) {
        Buffer& buf = conn->in_buf;
        if (buf.size() < 15)
            return 0;
        char str[16];
        memset(str, 0, 16);

        Response res(conn);
        while (buf.size() >= 15 && res.active()) {
            buf.copy_front((byte*) str, 15);
            buf.pop(15);
            std::stringstream ss;
            ss << hash(str);
            std::string result = ss.str();
            if (res.write_data((const byte*) result.c_str(),
                               result.length()) < 0) {
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
        //utils::logger.set_level(INFO);
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

