#ifndef _RPC_EXCEPTION_HXX_
#define _RPC_EXCEPTION_HXX_

#include "util.hxx"
#include "ptr.hxx"

#include <exception>
#include <string>

namespace SDN_Raft {

class RequestMessage;
class rpc_exception : public std::exception {
public:
    rpc_exception(const std::string& err, ptr<RequestMessage> req)
        : req_(req), err_(err.c_str()) {}

    __nocopy__(rpc_exception);
public:
    ptr<RequestMessage> req() const { return req_; }

    virtual const char* what() const throw() __override__ {
        return err_.c_str();
    }
private:
    ptr<RequestMessage> req_;
    std::string err_;
};

}

#endif 
