#include "Snapshot.hxx"
#include "ClusterConfig.hxx"
#include "Buffer.hxx"

namespace SDN_Raft {

ptr<Snapshot> Snapshot::deserialize(Buffer& buf) {
    BufferSerializer bs(buf);
    return deserialize(bs);
}

ptr<Snapshot> Snapshot::deserialize(BufferSerializer& bs) {
    type snp_type = static_cast<type>(bs.get_u8());
    ulong last_log_idx = bs.get_u64();
    ulong last_log_term = bs.get_u64();
    ulong size = bs.get_u64();
    ptr<ClusterConfig> conf( ClusterConfig::deserialize(bs) );
    return cs_new<Snapshot>(last_log_idx, last_log_term, conf, size, snp_type);
}

ptr<Buffer> Snapshot::serialize() {
    ptr<Buffer> conf_buf = last_config_->serialize();
    ptr<Buffer> buf = Buffer::alloc(conf_buf->size() + sz_ulong * 3 + sz_byte);
    buf->put((byte)type_);
    buf->put(last_log_idx_);
    buf->put(last_log_term_);
    buf->put(size_);
    buf->put(*conf_buf);
    buf->pos(0);
    return buf;
}

}

