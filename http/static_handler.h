// -*- mode: c++ -*-

#ifndef _STATIC_HANDLER_H_
#define _STATIC_HANDLER_H_

#include <string>

#include "utils/misc.h"
#include "http/http_wrapper.h"
#include "http/interface.h"

namespace pipeserv {

class StaticHttpHandler : public BaseHttpHandler
{
    std::string doc_root_;
public:
    static std::string remove_path_dots(const std::string& path);

    StaticHttpHandler();

    virtual void handle_request(HttpRequest& request, HttpResponse& response);
    virtual void load_param();

    void respond_zero_copy(const std::string& path, HttpRequest& request,
                           HttpResponse& resposne);
    void respond_error(HttpRequest& request, HttpResponse& response,
                       const HttpResponseStatus& error);
};

class StaticHttpHandlerFactory : public BaseHttpHandlerFactory
{
public:
    virtual BaseHttpHandler* create() const {
        return new StaticHttpHandler();
    }
    virtual std::string module_name() const {
        return std::string("static");
    }
    virtual std::string vender_name() const {
        return std::string("pipeserv");
    }
};

}

#endif /* _STATIC_HANDLER_H_ */
