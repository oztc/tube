#include "pch.h"

#include "http/configuration.h"
#include "utils/logger.h"

namespace pipeserv {

void
HandlerConfig::load_handlers(const Node& subdoc)
{
    // subdoc is an array
    for (size_t i = 0; i < subdoc.size(); i++) {
        load_handler(subdoc[i]);
    }
}

void
HandlerConfig::load_handler(const Node& subdoc)
{
    std::string name, module;
    subdoc["name"] >> name;
    subdoc["module"] >> module;
    BaseHttpHandler* handler = create_handler_instance(name, module);
    for (YAML::Iterator it = subdoc.begin(); it != subdoc.end(); ++it) {
        std::string key, value;
        it.first() >> key;
        it.second() >> value;
        if (key == "name" || key == "module")
            continue;
        handler->add_option(key, it.second());
    }
    handler->load_param();
}

HandlerConfig::HandlerConfig()
{}

HandlerConfig::~HandlerConfig()
{
    // FIXME: need to free all handlers?
}

BaseHttpHandler*
HandlerConfig::create_handler_instance(std::string name,
                                       std::string module)
{
    HandlerMap::iterator it = handlers_.find(name);
    if (it != handlers_.end()) {
        return it->second;
    }
    FactoryMap::iterator fac_it = factories_.find(module);
    if (fac_it != factories_.end()) {
        BaseHttpHandler* handler = fac_it->second->create();
        handlers_.insert(std::make_pair(name, handler));
        handler->set_name(name);
        return handler;
    }
    return NULL;
}

void
HandlerConfig::register_handler_factory(const BaseHttpHandlerFactory* factory)
{
    factories_[factory->module_name()] = factory;
}

BaseHttpHandler*
HandlerConfig::get_handler_instance(std::string name) const
{
    HandlerMap::const_iterator it = handlers_.find(name);
    if (it != handlers_.end()) {
        return it->second;
    }
    return NULL;
}

UrlRuleItem::UrlRuleItem(std::string reg)
    : regex(boost::xpressive::sregex::compile(reg))
{}

UrlRuleConfig::UrlRuleConfig()
{}

UrlRuleConfig::~UrlRuleConfig()
{}

void
UrlRuleConfig::load_url_rules(const Node& subdoc)
{
    for (YAML::Iterator it = subdoc.begin(); it != subdoc.end(); ++it) {
        std::string regex;
        it.first() >> regex;
        load_url_rule(regex, it.second());
    }
}

void
UrlRuleConfig::load_url_rule(std::string regex, const Node& subdoc)
{
    HandlerConfig& handler_cfg = HandlerConfig::instance();
    UrlRuleItem rule(regex);
    for (size_t i = 0; i < subdoc.size(); i++) {
        std::string name;
        subdoc[i] >> name;
        BaseHttpHandler* handler = handler_cfg.get_handler_instance(name);
        if (handler != NULL) {
            rule.handlers.push_back(handler);
        } else {
            LOG(WARNING, "Cannot find handler instance %s", name.c_str());
        }
    }
    rules_.push_back(rule);
}

const UrlRuleItem*
UrlRuleConfig::match_uri(std::string uri) const
{
    for (size_t i = 0; i < rules_.size(); i++) {
        if (boost::xpressive::regex_match(uri.begin(), uri.end(),
                                          rules_[i].regex)) {
            return &rules_[i];
        }
    }
    return NULL;
}

VHostConfig::VHostConfig()
{}

VHostConfig::~VHostConfig()
{}

void
VHostConfig::load_vhost_rules(const Node& subdoc)
{
    std::string host;
    UrlRuleConfig url_config;
    subdoc["domain"] >> host;
    url_config.load_url_rules(subdoc["url-rules"]);
    host_map_.insert(std::make_pair(host, url_config));
}

const UrlRuleItem*
VHostConfig::match_uri(std::string host, std::string uri) const
{
    HostMap::const_iterator it = host_map_.find(host);
    if (it == host_map_.end()) {
        it = host_map_.find("default");
    }
    return it->second.match_uri(uri);
}

ServerConfig::ServerConfig()
{}

ServerConfig::~ServerConfig()
{}

void
ServerConfig::load_config_file(const char* filename)
{
    std::ifstream fin(filename);
    YAML::Parser parser(fin);
    HandlerConfig& handler_cfg = HandlerConfig::instance();
    VHostConfig& host_cfg = VHostConfig::instance();

    Node doc;
    while (parser.GetNextDocument(doc)) {
        for (YAML::Iterator it = doc.begin(); it != doc.end(); ++it) {
            std::string key;
            it.first() >> key;
            if (key == "address") {
                it.second() >> address_;
            } else if (key == "port") {
                it.second() >> port_;
            } else if (key == "handlers") {
                handler_cfg.load_handlers(it.second());
            } else if (key == "host") {
                host_cfg.load_vhost_rules(it.second());
            }
            LOG(INFO, "ignore unsupported key %s", key.c_str());
        }
    }
}

}
