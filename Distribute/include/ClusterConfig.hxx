#ifndef _CLUSTER_CONFIG_HXX_
#define _CLUSTER_CONFIG_HXX_

#include "srv_config.hxx"

#include <list>
#include <vector>

namespace SDN_Raft {

class BufferSerializer;

class ClusterConfig {
public:
    ClusterConfig(ulong log_idx = 0L,
                   ulong prev_log_idx = 0L,
                   bool _ec = false)
        : log_idx_(log_idx)
        , prev_log_idx_(prev_log_idx)
        , async_replication_(_ec)
        , servers_()
        {}

    ~ClusterConfig() {
    }

    __nocopy__(ClusterConfig);

public:
    static ptr<ClusterConfig> deserialize(Buffer& buf);

    static ptr<ClusterConfig> deserialize(BufferSerializer& buf);

    ulong get_log_idx() const {
        return log_idx_;
    }

    void set_log_idx(ulong log_idx) {
        prev_log_idx_ = log_idx_;
        log_idx_ = log_idx;
    }

    ulong get_prev_log_idx() const {
        return prev_log_idx_;
    }

    const std::list<ptr<srv_config>>& get_servers() const {
        return servers_;
    }

    std::list<ptr<srv_config>>& get_servers() {
        return servers_;
    }

    ptr<srv_config> get_server(int id) const {
        for (auto& entry: servers_) {
            const ptr<srv_config>& srv = entry;
            if (srv->get_id() == id) {
                return srv;
            }
        }

        return ptr<srv_config>();
    }

    bool is_async_replication() const { return async_replication_; }

    void set_async_replication(bool flag) {
        async_replication_ = flag;
    }

    std::string get_user_ctx() const { return user_ctx_; }

    void set_user_ctx(const std::string& src) { user_ctx_ = src; }

    ptr<Buffer> serialize() const;

private:
    // Log index number of current config.
    ulong log_idx_;

    // Log index number of previous config.
    ulong prev_log_idx_;

    // `true` if asynchronous replication mode is on.
    bool async_replication_;

    // Custom config data given by user.
    std::string user_ctx_;

    // List of servers.
    std::list<ptr<srv_config>> servers_;
};

} // namespace SDN_Raft

#endif //_CLUSTER_CONFIG_HXX_
