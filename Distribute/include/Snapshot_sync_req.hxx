#ifndef _SNAPSHOT_SYNC_REQ_HXX_
#define _SNAPSHOT_SYNC_REQ_HXX_


#include "util.hxx"
#include "ptr.hxx"
#include "Snapshot.hxx"
#include "Buffer.hxx"

namespace SDN_Raft {

class Snapshot;
class Snapshot_sync_req {
public:
    Snapshot_sync_req(const ptr<Snapshot>& s,
                      ulong offset,
                      const ptr<Buffer>& buf,
                      bool done)
        : Snapshot_(s), offset_(offset), data_(buf), done_(done) {}

    __nocopy__(Snapshot_sync_req);

public:
    static ptr<Snapshot_sync_req> deserialize(Buffer& buf);

    static ptr<Snapshot_sync_req> deserialize(BufferSerializer& bs);

    Snapshot& get_Snapshot() const {
        return *Snapshot_;
    }

    ulong get_offset() const { return offset_; }
    void set_offset(const ulong src) { offset_ = src; }

    Buffer& get_data() const { return *data_; }

    bool is_done() const { return done_; }

    ptr<Buffer> serialize();
private:
    ptr<Snapshot> Snapshot_;
    ulong offset_;
    ptr<Buffer> data_;
    bool done_;
};

}

#endif 
