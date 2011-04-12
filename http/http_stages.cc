#include "pch.h"

#include "http/http_stages.h"
#include "http/connection.h"
#include "http/http_wrapper.h"
#include "http/configuration.h"
#include "core/stages.h"
#include "core/pipeline.h"
#include "utils/logger.h"

namespace tube {

int HttpConnectionFactory::kDefaultTimeout = 0;

Connection*
HttpConnectionFactory::create_connection(int fd)
{
    Connection* conn = new HttpConnection(fd);
    conn->set_timeout(kDefaultTimeout);
    return conn;
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
}

void
HttpParserStage::initialize()
{
    handler_stage_ = pipeline_.find_stage("http_handler");
}

HttpParserStage::~HttpParserStage()
{}

int
HttpParserStage::process_task(Connection* conn)
{
    Request req(conn);
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
    sched_ = new QueueScheduler();
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
            // mis-configured, send an error
            response.write_string("This url is not configured.");
            response.respond(
                HttpResponseStatus::kHttpResponseServiceUnavailable);
            continue;
        }
        if (request.keep_alive() && request.version_minor() == 0) {
            response.add_header("Connection", "Keep-Alive");
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
                HttpResponseStatus::kHttpResponseServiceUnavailable);
        }
        response.reset();
        if (!request.keep_alive()) {
            LOG(DEBUG, "active close after transfer finish");
            conn->close_after_finish = true;
            goto done;
        }
    }
    if (!client_requests.empty()) {
        LOG(DEBUG, "remaining req %lu", client_requests.size());
        sched_add(conn);
    }
done:
    return response.response_code();
}

}
