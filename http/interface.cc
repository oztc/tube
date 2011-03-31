#include "pch.h"

#include "http/configuration.h"
#include "http/interface.h"

namespace pipeserv {

void
BaseHttpHandler::add_option(const std::string& name, const std::string& value)
{
    OptionMap::iterator it = options_.find(name);
    if (it == options_.end()) {
        options_.insert(std::make_pair(name, value));
    } else {
        it->second = value;
    }
}

std::string
BaseHttpHandler::option(const std::string& name)
{
    OptionMap::iterator it = options_.find(name);
    if (it == options_.end()) {
        return "";
    }
    return it->second;
}

void
BaseHttpHandlerFactory::register_factory(BaseHttpHandlerFactory* factory)
{
    HandlerConfig::instance().register_handler_factory(factory);
}

}
