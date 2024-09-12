#ifndef _SNAPSHOT_SYNC_CTX_HXX_
#define _SNAPSHOT_SYNC_CTX_HXX_

#include "Awaiter.hxx"
#include "Timer.hxx"
#include "util.hxx"
#include "ptr.hxx"

#include <functional>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>

class EventAwaiter;

namespace SDN_Raft {

class peer;
class raft_server;
class resp_msg;
class rpc_exception;
class Snapshot;
class Snapshot_sync_ctx {
public:
    Snapshot_sync_ctx(const ptr<Snapshot>& s,
                      int peer_id,
                      ulong timeout_ms,
                      ulong offset = 0L);

    __nocopy__(Snapshot_sync_ctx);

public:
    const ptr<Snapshot>& get_Snapshot() const { return Snapshot_; }
    ulong get_offset() const { return offset_; }
    ulong get_obj_idx() const { return obj_idx_; }
    void*& get_user_snp_ctx() { return user_snp_ctx_; }

    void set_offset(ulong offset);
    void set_obj_idx(ulong obj_idx) { obj_idx_ = obj_idx; }
    void set_user_snp_ctx(void* _user_snp_ctx) { user_snp_ctx_ = _user_snp_ctx; }

    Timer& get_timer() { return timer_; }

private:
    void io_thread_loop();

    /**
     * Destination peer ID.
     */
    int32_t peer_id_;

    /**
     * Pointer to Snapshot.
     */
    ptr<Snapshot> Snapshot_;

    /**
     * Current cursor of Snapshot.
     * Can be used for either byte offset or object index,
     * but the legacy raw Snapshot (offset_) is deprecated.
     */
    union {
        ulong offset_;
        ulong obj_idx_;
    };

    /**
     * User-defined Snapshot context, given by the state machine.
     */
    void* user_snp_ctx_;

    /**
     * Timer to check Snapshot transfer timeout.
     */
    Timer timer_;
};

// Singleton class.
class Snapshot_io_mgr {
public:
    static Snapshot_io_mgr& instance() {
        static Snapshot_io_mgr mgr;
        return mgr;
    };

    /**
     * Push a Snapshot read request to the queue.
     *
     * @param r Raft server instance.
     * @param p Peer instance.
     * @param h Response handler.
     * @return `true` if succeeds (when there is no pending request for the same peer).
     */
    bool push(ptr<raft_server> r,
              ptr<peer> p,
              std::function< void(ptr<resp_msg>&, ptr<rpc_exception>&) >& h);

    /**
     * Invoke IO thread.
     */
    void invoke();

    /**
     * Drop all pending requests belonging to the given Raft instance.
     *
     * @param r Raft server instance.
     */
    void drop_reqs(raft_server* r);

    /**
     * Check if there is pending request for the given peer.
     *
     * @param r Raft server instance.
     * @param srv_id Server ID to check.
     * @return `true` if pending request exists.
     */
    bool has_pending_request(raft_server* r, int srv_id);

    /**
     * Shutdown the global Snapshot IO manager.
     */
    void shutdown();

private:
    struct io_queue_elem;

    Snapshot_io_mgr();

    ~Snapshot_io_mgr();

    void async_io_loop();

    bool push(ptr<io_queue_elem>& elem);

    /**
     * A dedicated thread for reading Snapshot object.
     */
    std::thread io_thread_;

    /**
     * Event awaiter for `io_thread_`.
     */
    ptr<EventAwaiter> io_thread_ea_;

    /**
     * `true` if we are closing this context.
     */
    std::atomic<bool> terminating_;

    /**
     * Request queue. Allow only one request per peer at a time.
     */
    std::list< ptr<io_queue_elem> > queue_;

    /**
     * Lock for `queue_`.
     */
    std::mutex queue_lock_;
};

}

#endif //_SNAPSHOT_SYNC_CTX_HXX_
