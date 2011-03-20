// -*- mode: c++ -*-

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

#include <yaml-cpp/yaml.h>
#include <boost/function.hpp>
#include <boost/xpressive/xpressive.hpp>

#include "http/interface.h"

namespace pipeserv {

typedef YAML::Node Node;

class HandlerConfig
{
    HandlerConfig();
    ~HandlerConfig();
public:
    static HandlerConfig& instance() {
        static HandlerConfig ins;
        return ins;
    }

    void register_handler_factory(const BaseHttpHandlerFactory* factory);

    BaseHttpHandler* create_handler_instance(std::string name,
                                             std::string module);
    BaseHttpHandler* get_handler_instance(std::string name) const;

    void load_handlers(const Node& subdoc);
    void load_handler(const Node& subdoc);
private:
    typedef std::map<std::string, const BaseHttpHandlerFactory*> FactoryMap;
    typedef std::map<std::string, BaseHttpHandler*> HandlerMap;
    FactoryMap factories_;
    HandlerMap handlers_;
};

struct UrlRuleItem
{
    boost::xpressive::sregex regex;
    typedef std::list<BaseHttpHandler*> HandlerChain;
    HandlerChain handlers;

    UrlRuleItem(std::string regex);
};

class UrlRuleConfig
{
public:
    UrlRuleConfig();
    ~UrlRuleConfig();
    void load_url_rules(const Node& subdoc);
    void load_url_rule(std::string regex, const Node& subdoc);

    const UrlRuleItem* match_uri(std::string url) const;

private:
    std::vector<UrlRuleItem> rules_;
};

class VHostConfig
{
    typedef std::map<std::string, UrlRuleConfig> HostMap;
    HostMap host_map_;
    VHostConfig();
    ~VHostConfig();
public:
    static VHostConfig& instance() {
        static VHostConfig ins;
        return ins;
    }

    void load_vhost_rules(const Node& subdoc);
    const UrlRuleItem* match_uri(std::string host, std::string uri) const;
};

class ServerConfig
{
    ServerConfig();
    ~ServerConfig();
public:
    static ServerConfig& instance() {
        static ServerConfig ins;
        return ins;
    }

    void load_config_file(const char* filename);

    std::string address() const { return address_; }
    std::string port() const { return port_; }
private:
    std::string address_;
    std::string port_; // port can be a server, keep it as a string

};

}

#endif /* _CONFIGURATION_H_ */
