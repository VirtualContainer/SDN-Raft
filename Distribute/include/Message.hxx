#ifndef _MSG_BASE_HXX_
#define _MSG_BASE_HXX_

#include "util.hxx"
#include <string>
#include <vector>

namespace SDN_Raft {

enum MessageType {
    request_vote_request            = 1,
    request_vote_response           = 2,
    append_entries_request          = 3,
    append_entries_response         = 4,
    client_request                  = 5,
    add_server_request              = 6,
    add_server_response             = 7,
    remove_server_request           = 8,
    remove_server_response          = 9,
    sync_log_request                = 10,
    sync_log_response               = 11,
    join_cluster_request            = 12,
    join_cluster_response           = 13,
    leave_cluster_request           = 14,
    leave_cluster_response          = 15,
    install_Snapshot_request        = 16,
    install_Snapshot_response       = 17,
    ping_request                    = 18,
    ping_response                   = 19,
    pre_vote_request                = 20,
    pre_vote_response               = 21,
    other_request                   = 22,
    other_response                  = 23,
    priority_change_request         = 24,
    priority_change_response        = 25,
    reconnect_request               = 26,
    reconnect_response              = 27,
    custom_notification_request     = 28,
    custom_notification_response    = 29,
};

inline bool is_valid_msg(MessageType type) {
    if ( type >= request_vote_request &&
         type <= custom_notification_response ) {
        return true;
    }
    return false;
}

// for tracing and debugging
inline std::string MessageType_to_string(MessageType type) {
    switch (type) {
    case request_vote_request:          return "request_vote_request";
    case request_vote_response:         return "request_vote_response";
    case append_entries_request:        return "append_entries_request";
    case append_entries_response:       return "append_entries_response";
    case client_request:                return "client_request";
    case add_server_request:            return "add_server_request";
    case add_server_response:           return "add_server_response";
    case remove_server_request:         return "remove_server_request";
    case remove_server_response:        return "remove_server_response";
    case sync_log_request:              return "sync_log_request";
    case sync_log_response:             return "sync_log_response";
    case join_cluster_request:          return "join_cluster_request";
    case join_cluster_response:         return "join_cluster_response";
    case leave_cluster_request:         return "leave_cluster_request";
    case leave_cluster_response:        return "leave_cluster_response";
    case install_Snapshot_request:      return "install_Snapshot_request";
    case install_Snapshot_response:     return "install_Snapshot_response";
    case ping_request:                  return "ping_request";
    case ping_response:                 return "ping_response";
    case pre_vote_request:              return "pre_vote_request";
    case pre_vote_response:             return "pre_vote_response";
    case other_request:                 return "other_request";
    case other_response:                return "other_response";
    case priority_change_request:       return "priority_change_request";
    case priority_change_response:      return "priority_change_response";
    case reconnect_request:             return "reconnect_request";
    case reconnect_response:            return "reconnect_response";
    case custom_notification_request:   return "custom_notification_request";
    case custom_notification_response:  return "custom_notification_response";
    default:
        return "unknown (" + std::to_string(static_cast<int>(type)) + ")";
    }
}

/*消息接口*/
class Message {
public:
    Message(ulong term, MessageType type, int src, int dst)
        : term_(term), type_(type), src_(src), dst_(dst) {}

    virtual ~Message() {}

    ulong get_term() const {
        return this->term_;
    }
    MessageType get_type() const {
        return this->type_;
    }
    int32 get_src() const {
        return this->src_;
    }
    int32 get_dst() const {
        return this->dst_;
    }

__nocopy__(Message);

private:
    ulong term_;
    MessageType type_;
    int32 src_;
    int32 dst_;
};

class LogEntry;
/*请求消息格式*/
class RequestMessage : public Message {
public:
    RequestMessage(ulong term,
            MessageType type,
            int32 src,
            int32 dst,
            ulong last_log_term,
            ulong last_log_idx,
            ulong commit_idx)
        : Message(term, type, src, dst)
        , last_log_term_(last_log_term)
        , last_log_idx_(last_log_idx)
        , commit_idx_(commit_idx)
        , log_entries_()
        { }

    virtual ~RequestMessage() __override__ { }

    __nocopy__(RequestMessage);

public:
    ulong get_last_log_idx() const {
        return last_log_idx_;
    }

    ulong get_last_log_term() const {
        return last_log_term_;
    }

    ulong get_commit_idx() const {
        return commit_idx_;
    }

    std::vector<ptr<LogEntry>>& log_entries() {
        return log_entries_;
    }

private:
    ulong last_log_term_;
    ulong last_log_idx_;
    ulong commit_idx_;
    std::vector<ptr<LogEntry>> log_entries_;
};

}
#endif 
