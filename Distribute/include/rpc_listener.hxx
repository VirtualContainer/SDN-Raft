#ifndef _RPC_LISTENER_HXX_
#define _RPC_LISTENER_HXX_

#include "util.hxx"
#include "ptr.hxx"

namespace SDN_Raft {

// for backward compatibility
class raft_server;
typedef raft_server msg_handler;

class rpc_listener {
__interface_body__(rpc_listener);
public:
    virtual void listen(ptr<msg_handler>& handler) = 0;
    virtual void stop() = 0;
    virtual void shutdown() {}
};

}

#endif 
