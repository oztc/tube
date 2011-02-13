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

    unsigned int hash(std::string str) {
        unsigned int sum = 0;
        for (int i = 0; i < str.size(); i++) {
            sum += str[i] << i;
        }
        return sum;
    }

    virtual int process_task(Connection* conn) {
        Buffer& buf = conn->in_buf;
        if (buf.size() < 15)
            return 0;
        Response res(conn);
        while (buf.size() >= 15 && res.active()) {
            std::string str(buf.ptr(), buf.ptr() + 15);
            std::stringstream ss;
            ss << hash(str);
            str = ss.str();
            //fprintf(stderr, "hash: %s\n", str.c_str());
            buf.pop(15);
            if (res.write_data((const byte*) str.c_str(), str.length()) < 0) {
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

