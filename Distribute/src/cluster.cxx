#include "ClusterConfig.hxx"
#include "Buffer.hxx"

namespace SDN_Raft {

ptr<Buffer> ClusterConfig::serialize() const {
    size_t sz = 2 * sz_ulong + sz_int + sz_byte;
    std::vector<ptr<Buffer>> srv_buffs;
    for (auto it = servers_.cbegin(); it != servers_.cend(); ++it) {
        ptr<Buffer> buf = (*it)->serialize();
        srv_buffs.push_back(buf);
        sz += buf->size();
    }
    // For aux string.
    sz += sz_int;
    sz += user_ctx_.size();

    ptr<Buffer> result = Buffer::alloc(sz);
    result->put(log_idx_);
    result->put(prev_log_idx_);
    result->put((byte)(async_replication_ ? 1 : 0));
    result->put((byte*)user_ctx_.data(), user_ctx_.size());
    result->put((int32)servers_.size());
    for (size_t i = 0; i < srv_buffs.size(); ++i) {
        result->put(*srv_buffs[i]);
    }

    result->pos(0);
    return result;
}

ptr<ClusterConfig> ClusterConfig::deserialize(Buffer& buf) {
    BufferSerializer bs(buf);
    return deserialize(bs);
}

ptr<ClusterConfig> ClusterConfig::deserialize(BufferSerializer& bs) {
    ulong log_idx = bs.get_u64();
    ulong prev_log_idx = bs.get_u64();

    byte ec_byte = bs.get_u8();
    bool ec = ec_byte ? true : false;

    size_t ctx_len;
    const byte* ctx_data = (const byte*)bs.get_bytes(ctx_len);
    std::string user_ctx = std::string((const char*)ctx_data, ctx_len);

    int32 cnt = bs.get_i32();
    ptr<ClusterConfig> conf = cs_new<ClusterConfig>(log_idx, prev_log_idx, ec);
    while (cnt -- > 0) {
        conf->get_servers().push_back(srv_config::deserialize(bs));
    }

    conf->set_user_ctx(user_ctx);

    return conf;
}

} // namespace SDN_Raft;

