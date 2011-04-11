#include "pch.h"

#include <unistd.h>

#include "http/static_handler.h"
#include "http/http_wrapper.h"
#include "http/configuration.h"
#include "http/http_stages.h"
#include "http/module.h"

#include "core/server.h"
#include "core/stages.h"
#include "core/wrapper.h"
#include "utils/logger.h"
#include "utils/exception.h"

namespace tube {

class WebServer : public Server
{
    HttpParserStage* parser_stage_;
    HttpHandlerStage* handler_stage_;
    size_t handler_stage_pool_size_;
public:
    WebServer(const char* address, const char* port) : Server(address, port) {
        parser_stage_ = new HttpParserStage();
        handler_stage_ = new HttpHandlerStage();
    }

    void initialize_stages() {
        Server::initialize_stages();
        parser_stage_->initialize();
        handler_stage_->initialize();
    }

    void start_all_threads() {
        parser_stage_->start_thread();
        for (size_t i = 0; i < handler_stage_pool_size_; i++) {
            handler_stage_->start_thread();
        }
        Server::start_all_threads();
    }

    void set_handler_stage_pool_size(size_t val) {
        handler_stage_pool_size_ = val;
    }

    virtual ~WebServer() {
        delete parser_stage_;
        delete handler_stage_;
    }
};

}

static std::string conf_file;
static std::string module_path;
static int global_uid = -1;

static void
show_usage(int argc, char* argv[])
{
    printf("Usage: %s -c config_file [ -m module_path -u uid ]\n", argv[0]);
    puts("");
    puts("  -c\t\t Specify the configuration file. Required.");
    puts("  -m\t\t Specify the module path. Optional.");
    puts("  -u\t\t Set uid before server starts. Optional.");
    puts("  -h\t\t Help");
    exit(-1);
}

static void
parse_opt(int argc, char* argv[])
{
    int opt = 0;
    while ((opt = getopt(argc, argv, "c:m:u:h")) != -1) {
        switch (opt) {
        case 'c':
            conf_file = std::string(optarg);
            break;
        case 'm':
            module_path = std::string(optarg);
            break;
        case 'u':
            global_uid = atoi(optarg);
            break;
        case 'h':
            show_usage(argc, argv);
            break;
        default:
            fprintf(stderr, "Try `%s -h' for help.\n", argv[0]);
            exit(-1);
            break;
        }
    }
    if (conf_file == "") {
        fprintf(stderr, "Must specify the configuration file.\n");
        exit(-1);
    }
}

static void
load_modules()
{
    tube_module_load_dir(module_path.c_str());
    tube_module_initialize_all();
}

using namespace tube;

int
main(int argc, char* argv[])
{
    parse_opt(argc, argv);
    if (global_uid >= 0) {
        if (setuid(global_uid) < 0) {
            perror("setuid");
            exit(-1);
        }
    }
    load_modules();
    ServerConfig& cfg = ServerConfig::instance();
    try {
        cfg.load_config_file(conf_file.c_str());
        WebServer server(cfg.address().c_str(), cfg.port().c_str());
        if (cfg.read_stage_pool_size() > 0) {
            server.set_read_stage_pool_size(cfg.read_stage_pool_size());
        }
        if (cfg.write_stage_pool_size() > 0) {
            server.set_write_stage_pool_size(cfg.write_stage_pool_size());
        }
        if (cfg.recycle_threshold() > 0) {
            server.set_recycle_threshold(cfg.recycle_threshold());
        }
        if (cfg.handler_stage_pool_size() > 0) {
            server.set_handler_stage_pool_size(cfg.handler_stage_pool_size());
        }
        server.initialize_stages();
        server.start_all_threads();
        server.listen(cfg.listen_queue_size());
        server.main_loop();
    } catch (utils::SyscallException ex) {
        fprintf(stderr, "Cannot start server: %s\n", ex.what());
        exit(-1);
    }

    return 0;
}
