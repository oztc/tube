// -*- mode: c++ -*-

#ifndef _STATIC_HANDLER_H_
#define _STATIC_HANDLER_H_

#include <string>

#include "utils/misc.h"
#include "http/http_wrapper.h"
#include "http/interface.h"
#include "http/io_cache.h"

namespace pipeserv {

class StaticHttpHandler : public BaseHttpHandler
{
    std::string doc_root_;

    IOCache io_cache_;
public:
    static std::string remove_path_dots(const std::string& path);

    StaticHttpHandler();

    virtual void handle_request(HttpRequest& request, HttpResponse& response);
    virtual void load_param();

    void respond_file_content(const std::string& path, struct stat64 stat,
                              HttpRequest& request, HttpResponse& resposne);

    void respond_error(const HttpResponseStatus& error,
                       HttpRequest& request, HttpResponse& response);

    void respond_directory_list(const std::string& path,
                                const std::string& href_path,
                                HttpRequest& request,
                                HttpResponse& response);
private:
    bool validate_client_cache(const std::string& path, struct stat64 stat,
                               std::string etag, HttpRequest& request);
    int  try_open_file(const std::string& path, HttpRequest& request,
                       HttpResponse& response);
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
