// -*- mode: c++ -*-

#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <map>
#include <string>

#include "http/http_wrapper.h"

namespace pipeserv {

// gateway interface
class BaseHttpHandler
{
public:
    virtual void handle_request(HttpRequest& request,
                                HttpResponse& response) = 0;

    virtual void load_param() {}

    void set_name(std::string name) { name_ = name; }
    std::string name() const { return name_; }

    std::string option(std::string name);
    void add_option(std::string name, std::string value);

private:
    typedef std::map<std::string, std::string> OptionMap;
    OptionMap options_;

    std::string name_;
};

class BaseHttpHandlerFactory
{
public:
    virtual BaseHttpHandler* create() const = 0;
    virtual std::string module_name() const = 0;
    virtual std::string vender_name() const = 0;
};

}

#endif /* _INTERFACE_H_ */
