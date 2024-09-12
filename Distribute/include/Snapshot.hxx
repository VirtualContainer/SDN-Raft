#ifndef _SNAPSHOT_HXX_
#define _SNAPSHOT_HXX_

#include "util.hxx"
#include "ptr.hxx"

namespace SDN_Raft {

class Buffer;
class BufferSerializer;
class ClusterConfig;

class Snapshot {
public:
    enum type : uint8_t {
        raw_binary = 0x1,
        logical_object = 0x2,
    };

    Snapshot(ulong last_log_idx,
             ulong last_log_term,
             const ptr<ClusterConfig>& last_config,
             ulong size = 0,
             type _type = logical_object)
        : last_log_idx_(last_log_idx)
        , last_log_term_(last_log_term)
        , size_(size)
        , last_config_(last_config)
        , type_(_type)
        {}

    __nocopy__(Snapshot);

public:
    /*获取和设置元数据信息*/
    ulong get_last_log_idx() const {
        return last_log_idx_;
    }
    ulong get_last_log_term() const {
        return last_log_term_;
    }
    ulong size() const {
        return size_;
    }
    void set_size(ulong size) {
        size_ = size;
    }
    type get_type() const {
        return type_;
    }
    void set_type(type src) {
        type_ = src;
    }
    const ptr<ClusterConfig>& get_last_config() const {
        return last_config_;
    }

    /*序列化与反序列化方法*/
    static ptr<Snapshot> deserialize(Buffer& buf);
    static ptr<Snapshot> deserialize(BufferSerializer& bs);
    ptr<Buffer> serialize();

private:
    ulong last_log_idx_;
    ulong last_log_term_;
    ulong size_;
    ptr<ClusterConfig> last_config_;
    type type_;
};

}

#endif
