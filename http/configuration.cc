#include <fstream>

#include "http/configuration.h"
#include "utils/logger.h"

namespace pipeserv {

void
HandlerConfig::load_handlers(const Node& subdoc)
{
    // subdoc is an array
    for (int i = 0; i < subdoc.size(); i++) {
        load_handler(subdoc[i]);
    }
}

void
HandlerConfig::load_handler(const Node& subdoc)
{
    std::string name = subdoc["name"], module = subdoc["module"];
    BaseHttpHandler* handler = create_handler_instance(name, module);
    for (YAML::Iterator it = subdoc.begin(); it != subdoc.end(); ++it) {
        std::string key = it.first();
        if (key == "name" || key == "module")
            continue;
        handler->add_option(key, it.second());
    }
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
        std::string regex = it.first();
        load_url_rule(regex, it.second());
    }
}

void
UrlRuleConfig::load_url_rule(std::string regex, const Node& subdoc)
{
    HandlerConfig& handler_cfg = HandlerConfig::instance();
    UrlRuleItem rule(regex);
    for (int i = 0; i < subdoc.size(); i++) {
        std::string name = subdoc[i];
        BaseHttpHandler* handler =  handler_cfg.get_handler_instance(name);
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
    for (int i = 0; i < rules_.size(); i++) {
        // TODO: detect wether regex of the rule matches uri
        if (boost::xpressive::regex_match(uri.begin(), uri.end(),
                                          rules_[i].regex)) {
            return &rules_[i];
        }
    }
    return NULL;
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
    UrlRuleConfig& url_rule_cfg = UrlRuleConfig::instance();

    Node doc;
    while (parser.GetNextDocument(doc)) {
        for (YAML::Iterator it = doc.begin(); it != doc.end(); ++it) {
            if (it.first() == "address") {
                it.second() >> address_;
            } else if (it.first() == "port") {
                it.second() >> port_;
            } else if (it.first() == "handlers") {
                handler_cfg.load_handlers(it.second());
            } else if (it.first() == "url-rules") {
                url_rule_cfg.load_url_rules(it.second());
            }
        }
    }
}

}
