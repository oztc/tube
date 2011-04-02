#include "http/configuration.h"

using namespace tube;

int
main(int argc, char *argv[])
{
    ServerConfig& conf = ServerConfig::instance();
    conf.load_config_file("test/test-conf.yaml");
    assert(conf.address() == "127.0.0.1");
    assert(conf.port() == "80");

    return 0;
}
