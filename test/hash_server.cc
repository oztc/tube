#include <string>
#include <iostream>

#include "utils/logger.h"
#include "core/stages.h"
#include "core/server.h"
#include "core/wrapper.h"

using namespace pipeserv;

class HashParser : public ParserStage
{
    //Stage* out_stage_;
public:

    virtual void initialize() {
        //out_stage_ = Pipeline::instance().find_stage("poll_out");
    }

    unsigned int hash(std::string str) {
        unsigned int sum = 0;
        for (int i = 0; i < str.size(); i++) {
            sum += str[i] << i;
        }
        return sum;
    }

    virtual void process_task(Connection* conn) {
        Buffer& buf = conn->in_buf;
        Pipeline& pipeline = Pipeline::instance();
        if (buf.size() < 15)
            return;
        Response res(conn);
        while (buf.size() >= 15 && res.active()) {
            std::string str(buf.ptr(), buf.ptr() + 15);
            std::stringstream ss;
            ss << hash(str);
            str = ss.str();
            std::cout << "hash: " << str << std::endl;
            buf.pop(15);
            if (res.write_data((const byte*) str.c_str(), str.length()) < 0) {
                res.close(); // close connection
            }
        }
        //conn->async_write_lock.lock();
        //out_stage_->sched_add(conn);
        //conn->async_write_lock.lock(); // bla...blocked
        //conn->async_write_lock.unlock();
    }
};

class HashServer : public Server
{
    PollInStage* read_stage_;
    PollOutStage* out_stage_;
    HashParser*  parse_stage_;
    RecycleStage* recycle_stage_;
public:
    HashServer() : Server("0.0.0.0", "7000") {
        utils::set_fdtable_size(8096);
        utils::logger.set_level(DEBUG);
        read_stage_ = new PollInStage();
        //out_stage_ = new PollOutStage();
        recycle_stage_ = new RecycleStage();
        parse_stage_ = new HashParser();

        read_stage_->initialize();
        //out_stage_->initialize();
        recycle_stage_->initialize();
        parse_stage_->initialize();
    }

    void start() {
        recycle_stage_->start_thread();
        read_stage_->start_thread();
        //out_stage_->start_thread();
        for (int i = 0; i < 2; i++) {
            parse_stage_->start_thread();
        }
    }

    virtual ~HashServer() {
        delete read_stage_;
        //delete out_stage_;
        delete parse_stage_;
        delete recycle_stage_;
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

