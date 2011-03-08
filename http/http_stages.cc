#include "pch.h"

#include "http/http_stages.h"
#include "http/connection.h"
#include "http/http_wrapper.h"
#include "http/configuration.h"
#include "core/stages.h"
#include "core/pipeline.h"
#include "utils/logger.h"

namespace pipeserv {

class HttpConnectionFactory : public ConnectionFactory
{
public:
    virtual Connection* create_connection(int fd);
    virtual void        destroy_connection(Connection* conn);
};

Connection*
HttpConnectionFactory::create_connection(int fd)
{
    return new HttpConnection(fd);
}

void
HttpConnectionFactory::destroy_connection(Connection* conn)
{
    delete conn;
}

HttpParserStage::HttpParserStage()
{
    // replace the connection factory
    pipeline_.set_connection_factory(new HttpConnectionFactory());
    handler_stage_ = pipeline_.find_stage("http_handler");
}

HttpParserStage::~HttpParserStage()
{}

int
HttpParserStage::process_task(Connection* conn)
{
    HttpConnection* http_connection = (HttpConnection*) conn;
    if (!http_connection->do_parse()) {
        // FIXME: if the protocol client sent is not HTTP, is it OK to close
        // the connection right away?
        LOG(WARNING, "corrupted protocol from %s. closing...",
            conn->address.address_string().c_str());
        conn->active_close();
    }
    if (!http_connection->get_request_data_list().empty()) {
        // add it into the next stage
        handler_stage_->sched_add(conn);
    }
    return 0; // release the lock whatever happened
}

const int HttpHandlerStage::kMaxContinuesRequestNumber = 3;

HttpHandlerStage::HttpHandlerStage()
    : Stage("http_handler")
{
    // default_chain_.push_back();
}

HttpHandlerStage::~HttpHandlerStage()
{}

int
HttpHandlerStage::process_task(Connection* conn)
{
    HttpConnection* http_connection = (HttpConnection*) conn;
    std::list<BaseHttpHandler*> chain;
    std::list<HttpRequestData>& client_requests =
        http_connection->get_request_data_list();
    HttpResponse response(conn);

    for (int i = 0; i < kMaxContinuesRequestNumber; i++) {
        if (client_requests.empty())
            break;
        HttpRequest request(conn, client_requests.front());
        client_requests.pop_front();
        if (request.url_rule_item()) {
            chain = request.url_rule_item()->handlers;
        } else {
            chain = default_chain_;
        }
        for (UrlRuleItem::HandlerChain::iterator it = chain.begin();
             it != chain.end(); ++it) {
            BaseHttpHandler* handler = *it;
            handler->handle_request(request, response);
            if (response.is_responded())
                break;
        }
        if (!response.is_responded()) {
            response.respond(
                HttpResponseStatus::kHttpResponseInternalServerError);
        }
        response.reset();
    }
}

}
