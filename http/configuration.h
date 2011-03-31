// -*- mode: c++ -*-

#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

#include <yaml-cpp/yaml.h>

#include "http/interface.h"
#include "http/connection.h"

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

    BaseHttpHandler* create_handler_instance(const std::string& name,
                                             const std::string& module);
    BaseHttpHandler* get_handler_instance(const std::string& name) const;

    void load_handlers(const Node& subdoc);
    void load_handler(const Node& subdoc);
private:
    typedef std::map<std::string, const BaseHttpHandlerFactory*> FactoryMap;
    typedef std::map<std::string, BaseHttpHandler*> HandlerMap;
    FactoryMap factories_;
    HandlerMap handlers_;
};

class UrlRuleItemMatcher;

struct UrlRuleItem
{
    typedef std::list<BaseHttpHandler*> HandlerChain;
    HandlerChain handlers;
    UrlRuleItemMatcher* matcher;

    UrlRuleItem(const std::string& type, const Node& subdoc);
    virtual ~UrlRuleItem();
};

class UrlRuleConfig
{
public:
    UrlRuleConfig();
    ~UrlRuleConfig();
    void load_url_rules(const Node& subdoc);
    void load_url_rule(const Node& subdoc);

    const UrlRuleItem* match_uri(HttpRequestData& req_ref) const;

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
    const UrlRuleItem* match_uri(const std::string& host,
                                 HttpRequestData& req_ref) const;
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
    int read_stage_pool_size() const { return read_stage_pool_size_; }
    int write_stage_pool_size() const {   return write_stage_pool_size_; }
    int recycle_threshold() const { return recycle_threshold_; }
    int handler_stage_pool_size() const { return handler_stage_pool_size_; }
    int listen_queue_size() const { return listen_queue_size_; }

private:
    std::string address_;
    std::string port_; // port can be a server, keep it as a string

    int read_stage_pool_size_;
    int write_stage_pool_size_;
    int recycle_threshold_;
    int handler_stage_pool_size_;
    int listen_queue_size_;
};

}

#endif /* _CONFIGURATION_H_ */
