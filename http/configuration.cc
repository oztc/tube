#include "pch.h"

#include <boost/xpressive/xpressive.hpp>

#include "http/configuration.h"
#include "http/http_stages.h"
#include "utils/logger.h"

namespace tube {

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
        handler->add_option(key, value);
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
HandlerConfig::create_handler_instance(const std::string& name,
                                       const std::string& module)
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
HandlerConfig::get_handler_instance(const std::string& name) const
{
    HandlerMap::const_iterator it = handlers_.find(name);
    if (it != handlers_.end()) {
        return it->second;
    }
    return NULL;
}

class UrlRuleItemMatcher
{
public:
    virtual ~UrlRuleItemMatcher() {}
    virtual bool match(HttpRequestData& req_ref) = 0;
};

class RegexUrlRuleItemMatcher : public UrlRuleItemMatcher
{
    boost::xpressive::sregex regex_;
public:
    RegexUrlRuleItemMatcher(const std::string& regex)
        : UrlRuleItemMatcher() {
        regex_ = boost::xpressive::sregex::compile(regex);
    }

    virtual bool match(HttpRequestData& req_ref) {
        const std::string& uri = req_ref.uri;
        return boost::xpressive::regex_match(uri.begin(), uri.end(), regex_);
    }
};

class PrefixUrlRuleItemMatcher : public UrlRuleItemMatcher
{
    std::string prefix_;
public:
    PrefixUrlRuleItemMatcher(const std::string& prefix)
        : UrlRuleItemMatcher(), prefix_(prefix) {}

    virtual bool match(HttpRequestData& req_ref) {
        std::string& path = req_ref.path;
        std::string& uri = req_ref.uri;
        if (path.length() < prefix_.length()) {
            return false;
        }
        if (path.substr(0, prefix_.length()) != prefix_) {
            return false;
        }
        path.erase(path.begin(), path.begin() + prefix_.length());
        uri.erase(uri.begin(), uri.begin() + prefix_.length());
        return true;
    }
};

UrlRuleItem::UrlRuleItem(const std::string& type, const Node& subdoc)
{
    if (type == "prefix") {
        std::string prefix;
        subdoc["prefix"] >> prefix;
        matcher = new PrefixUrlRuleItemMatcher(prefix);
    } else if (type == "regex") {
        std::string regex;
        subdoc["regex"] >> regex;
        matcher = new RegexUrlRuleItemMatcher(regex);
    } else {
        matcher = NULL;
    }
}

UrlRuleItem::~UrlRuleItem()
{}

UrlRuleConfig::UrlRuleConfig()
{}

UrlRuleConfig::~UrlRuleConfig()
{}

void
UrlRuleConfig::load_url_rules(const Node& subdoc)
{
    for (size_t i = 0; i < subdoc.size(); i++) {
        load_url_rule(subdoc[i]);
    }
}

void
UrlRuleConfig::load_url_rule(const Node& subdoc)
{
    std::string type, opt;
    subdoc["type"] >> type;
    UrlRuleItem rule(type, subdoc);

    const Node& chaindoc = subdoc["chain"];
    HandlerConfig& handler_cfg = HandlerConfig::instance();
    for (size_t i = 0; i < chaindoc.size(); i++) {
        std::string name;
        chaindoc[i] >> name;
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
UrlRuleConfig::match_uri(HttpRequestData& req_ref) const
{
    for (size_t i = 0; i < rules_.size(); i++) {
        LOG(DEBUG, "matching uri %lu/%lu", i, rules_.size());
        if (rules_[i].matcher == NULL || rules_[i].matcher->match(req_ref)) {
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

static std::string
parse_host(const std::string& host)
{
    size_t pos = host.find(':');
    if (pos != std::string::npos) {
        return host.substr(pos);
    }
    return host;
}

const UrlRuleItem*
VHostConfig::match_uri(const std::string& host, HttpRequestData& req_ref) const
{
    HostMap::const_iterator it = host_map_.find(parse_host(host));
    if (it == host_map_.end()) {
        it = host_map_.find("default");
    }
    return it->second.match_uri(req_ref);
}

ServerConfig::ServerConfig()
    : read_stage_pool_size_(0), write_stage_pool_size_(0),
      recycle_threshold_(0), handler_stage_pool_size_(0)
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
            std::string key, value;
            it.first() >> key;
            if (key == "address") {
                it.second() >> address_;
            } else if (key == "port") {
                it.second() >> port_;
            } else if (key == "handlers") {
                handler_cfg.load_handlers(it.second());
            } else if (key == "host") {
                host_cfg.load_vhost_rules(it.second());
            } else if (key == "read_stage_pool_size") {
                it.second() >> value;
                read_stage_pool_size_ = atoi(value.c_str());
            } else if (key == "write_stage_pool_size") {
                it.second() >> value;
                write_stage_pool_size_ = atoi(value.c_str());
            } else if (key == "recycle_threshold") {
                it.second() >> value;
                recycle_threshold_ = atoi(value.c_str());
            } else if (key == "handler_stage_pool_size") {
                it.second() >> value;
                handler_stage_pool_size_ = atoi(value.c_str());
            } else if (key == "listen_queue_size") {
                it.second() >> value;
                listen_queue_size_ = atoi(value.c_str());
            } else if (key == "idle_timeout") {
                it.second() >> value;
                HttpConnectionFactory::kDefaultTimeout = atoi(value.c_str());
            }
            LOG(INFO, "ignore unsupported key %s", key.c_str());
        }
    }
}

}
