#include "pch.h"

#include "http/static_handler.h"
#include "http/http_wrapper.h"
#include "http/configuration.h"
#include "http/http_stages.h"

#include "core/server.h"
#include "core/stages.h"
#include "core/wrapper.h"
#include "utils/logger.h"

using namespace tube;

class WebServer : public Server
{
    HttpParserStage* parser_stage_;
    HttpHandlerStage* handler_stage_;
    size_t handler_stage_pool_size_;
public:
    WebServer(const char* address, const char* port) : Server(address, port) {
        utils::logger.set_level(DEBUG);
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

// instanlized the factories

static StaticHttpHandlerFactory static_handler_factory;

int
main(int argc, char *argv[])
{
    ServerConfig& cfg = ServerConfig::instance();
    BaseHttpHandlerFactory::register_factory(&static_handler_factory);

    cfg.load_config_file("./test/test-conf.yaml");
    WebServer server(cfg.address().c_str(), cfg.port().c_str());
    if (cfg.read_stage_pool_size() > 0) {
        server.set_read_stage_pool_size((size_t) cfg.read_stage_pool_size());
    }
    if (cfg.write_stage_pool_size() > 0) {
        server.set_write_stage_pool_size((size_t) cfg.write_stage_pool_size());
    }
    if (cfg.recycle_threshold() > 0) {
        server.set_recycle_threshold((size_t) cfg.recycle_threshold());
    }
    if (cfg.handler_stage_pool_size() > 0) {
        server.set_handler_stage_pool_size(
            (size_t) cfg.handler_stage_pool_size());
    }
    server.initialize_stages();
    server.start_all_threads();
    server.listen(cfg.listen_queue_size());
    server.main_loop();
    return 0;
}
