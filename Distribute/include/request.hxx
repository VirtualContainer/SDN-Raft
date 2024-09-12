#pragma once

#include "async.hxx"
#include "Buffer.hxx"
#include "Awaiter.hxx"
#include "Timer.hxx"
#include "ptr.hxx"
#include "raft_server.hxx"

namespace SDN_Raft {

struct raft_server::commit_ret_elem {
    commit_ret_elem()
        : ret_value_(nullptr)
        , result_code_(cmd_result_code::OK)
        , async_result_(nullptr)
        , callback_invoked_(false)
        {}

    ~commit_ret_elem() {}

    ulong idx_;
    EventAwaiter awaiter_;
    Timer timer_;
    ptr<Buffer> ret_value_;
    cmd_result_code result_code_;
    ptr< cmd_result< ptr<Buffer> > > async_result_;
    bool callback_invoked_;
};

} // namespace SDN_Raft;

