#ifndef _RPC_CLIENT_FACTORY_HXX_
#define _RPC_CLIENT_FACTORY_HXX_

#include "rpc_cli.hxx"

namespace SDN_Raft {

class rpc_client_factory {
    __interface_body__(rpc_client_factory);

public:
    virtual ptr<rpc_client> create_client(const std::string& endpoint) = 0;
};

}

#endif
