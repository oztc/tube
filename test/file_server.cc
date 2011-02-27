#include <string>
#include <iostream>
#include <cassert>

#include "utils/logger.h"
#include "utils/misc.h"
#include "utils/exception.h"

#include "core/stages.h"
#include "core/server.h"
#include "core/wrapper.h"

using namespace pipeserv;

class FilenameRequest : public Request
{
    std::list<std::string> filenames_;
public:
    FilenameRequest(Connection* conn) : Request(conn) {}

    void add_filename(std::string str) { filenames_.push_back(str); }
    bool empty() const { return filenames_.empty(); }

    std::string next_filename() {
        std::string res = filenames_.front();
        filenames_.pop_front();
        return res;
    }
};

class FilenameParser : public ParserStage
{
public:
    virtual int process_task(Connection* conn) {
        FilenameRequest request(conn);
        Response response(conn);
        Buffer& buf = conn->in_stream.buffer();
        std::string tmp_str;
        size_t total_size = 0;

        for (Buffer::PageIterator it = buf.page_begin(); it != buf.page_end();
             ++it) {
            size_t len;
            char* ptr = (char*) buf.get_page_segment(*it, &len);
            if (ptr == NULL) break;
            char* chrpos = strchr(ptr, '\n');
            if (chrpos == NULL) {
                tmp_str += std::string(ptr, ptr + len);
            } else {
                tmp_str += std::string(ptr, chrpos);
                total_size += tmp_str.size() + 1;
                request.add_filename(tmp_str);
                tmp_str = std::string(chrpos + 1, ptr + len);
            }
        }
        buf.pop(total_size);
        printf("processed %d remain %d bytes\n", total_size, buf.size());

        handle_request(request, response);
        return response.response_code();
    }

    void handle_request(FilenameRequest& req, Response& res) {
        while (!req.empty()) {
            std::string filename = req.next_filename();
            try {
                res.write_file(filename, 0, -1);
            } catch (const utils::FileOpenError& ex) {
                LOG(WARNING, "%s: %s", filename.c_str(), ex.what());
            }
        }
        puts("done");
    }
};

class FileServer : public Server
{
    FilenameParser* parser_stage_;
public:
    FileServer() : Server("0.0.0.0", "7000") {
        utils::logger.set_level(DEBUG);

        parser_stage_ = new FilenameParser();
        parser_stage_->initialize();

        initialize_stages();
    }

    void start() {
        parser_stage_->start_thread();
    }

    virtual ~FileServer() {
        delete parser_stage_;
    }
};

static FileServer server;

int
main(int argc, char *argv[])
{
    server.start();
    server.listen(128);

    server.main_loop();
    return 0;
}
