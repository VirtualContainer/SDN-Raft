#ifndef _SRV_CONFIG_HXX_
#define _SRV_CONFIG_HXX_

#include "util.hxx"
#include "Buffer.hxx"
#include "ptr.hxx"

#include <string>

namespace SDN_Raft {

class BufferSerializer;

class srv_config {
public:
    // WARNING: Please see the comment at raft_server::raft_server(...).
    const static int32 INIT_PRIORITY = 1;

    srv_config(int32 id, const std::string& endpoint)
        : id_(id)
        , dc_id_(0)
        , endpoint_(endpoint)
        , learner_(false)
        , priority_(INIT_PRIORITY)
        {}

    srv_config(int32 id,
               int32 dc_id,
               const std::string& endpoint,
               const std::string& aux,
               bool learner,
               int32 priority = INIT_PRIORITY)
        : id_(id)
        , dc_id_(dc_id)
        , endpoint_(endpoint)
        , aux_(aux)
        , learner_(learner)
        , priority_(priority)
        {}

    __nocopy__(srv_config);

public:
    static ptr<srv_config> deserialize(Buffer& buf);

    static ptr<srv_config> deserialize(BufferSerializer& bs);

    int32 get_id() const { return id_; }

    int32 get_dc_id() const { return dc_id_; }

    const std::string& get_endpoint() const { return endpoint_; }

    const std::string& get_aux() const { return aux_; }

    bool is_learner() const { return learner_; }

    int32 get_priority() const { return priority_; }

    void set_priority(const int32 new_val) { priority_ = new_val; }

    ptr<Buffer> serialize() const;

private:
    /**
     * ID of this server, should be positive number.
     */
    int32 id_;

    /**
     * ID of datacenter where this server is located.
     * 0 if not used.
     */
    int32 dc_id_;

    /**
     * Endpoint (address + port).
     */
    std::string endpoint_;

    /**
     * Custom string given by user.
     * WARNING: It SHOULD NOT contain NULL character,
     *          as it will be stored as a C-style string.
     */
    std::string aux_;

    /**
     * `true` if this node is learner.
     * Learner will not initiate or participate in leader election.
     */
    bool learner_;

    /**
     * Priority of this node.
     * 0 will never be a leader.
     */
    int32 priority_;
};

} // namespace SDN_Raft

#endif
