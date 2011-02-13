#include <string>
#include <iostream>

#include "utils/logger.h"
#include "utils/misc.h"
#include "core/stages.h"
#include "core/server.h"
#include "core/wrapper.h"

using namespace pipeserv;

class PingPongParser : public ParserStage
{
public:
    virtual int process_task(Connection* conn) {
        Buffer& buf = conn->in_buf;
        if (buf.size() < 4096) return 0;
        Response res(conn);
        if (res.write_data(buf.ptr(), buf.size()) < 0) {
            res.close();
        }
        buf.clear();
        return res.response_code();
    }
};

class PingPongServer : public Server
{
    PingPongParser* parser_stage_;
public:
    PingPongServer() : Server("0.0.0.0", "7000") {
        utils::set_fdtable_size(20000);
        //utils::logger.set_level(DEBUG);
        parser_stage_ = new PingPongParser();
        parser_stage_->initialize();

        initialize_stages();
    }

    void start() {
        for (int i = 0; i < 2; i++) {
            parser_stage_->start_thread();
        }
    }

    virtual ~PingPongServer() {
        delete parser_stage_;
    }
};

static PingPongServer server;

int
main(int argc, char *argv[])
{
    server.start();
    server.listen(128);

    server.main_loop();
    return 0;
}

