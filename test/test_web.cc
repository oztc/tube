#include "pch.h"

#include "http/static_handler.h"
#include "http/http_wrapper.h"
#include "http/configuration.h"
#include "http/http_stages.h"

#include "core/server.h"
#include "core/stages.h"
#include "core/wrapper.h"
#include "utils/logger.h"

using namespace pipeserv;

class WebServer : public Server
{
    HttpParserStage* parser_stage_;
    HttpHandlerStage* handler_stage_;
public:
    WebServer(const char* address, const char* port) : Server(address, port) {
        utils::logger.set_level(DEBUG);
        parser_stage_ = new HttpParserStage();
        handler_stage_ = new HttpHandlerStage();

        initialize_stages();
        parser_stage_->initialize();
        handler_stage_->initialize();

        parser_stage_->start_thread();
        handler_stage_->start_thread();
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
    server.listen(128);
    server.main_loop();
    return 0;
}
