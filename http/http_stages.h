// -*- mode: c++ -*-

#ifndef _HTTP_STAGES_H_
#define _HTTP_STAGES_H_

#include "core/stages.h"
#include "http/interface.h"

namespace pipeserv {

class HttpParserStage : public ParserStage
{
    Stage* handler_stage_;
public:
    HttpParserStage();
    virtual ~HttpParserStage();
protected:
    int process_task(Connection* conn);
};

class HttpHandlerStage : public Stage
{
public:
    static const int kMaxContinuesRequestNumber;
    
    HttpHandlerStage();
    virtual ~HttpHandlerStage();
protected:
    int process_task(Connection* conn);
private:
    std::list<BaseHttpHandler*> default_chain_;
};

}

#endif /* _HTTP_STAGES_H_ */
