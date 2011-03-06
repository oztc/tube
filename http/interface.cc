#include "http/interface.h"

namespace pipeserv {

void
BaseHttpHandler::add_option(std::string name, std::string value)
{
    OptionMap::iterator it = options_.find(name);
    if (it == options_.end()) {
        options_.insert(std::make_pair(name, value));
    } else {
        it->second = value;
    }
}

std::string
BaseHttpHandler::option(std::string name)
{
    OptionMap::iterator it = options_.find(name);
    if (it == options_.end()) {
        return "";
    }
    return it->second;
}

}
