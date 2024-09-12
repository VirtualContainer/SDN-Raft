#ifndef _STATE_MACHINE_HXX_
#define _STATE_MACHINE_HXX_

#include "async.hxx"
#include "Buffer.hxx"
#include "util.hxx"
#include "ptr.hxx"

#include <unordered_map>

namespace SDN_Raft {

class ClusterConfig;
class Snapshot;

class state_machine {
    __interface_body__(state_machine);

public:
    struct ext_op_params {
        ext_op_params(ulong _log_idx,
                      ptr<Buffer>& _data)
            : log_idx(_log_idx)
            , data(_data)
            {}
        ulong log_idx;
        ptr<Buffer>& data;
        // May add more parameters in the future.
    };

    /**
     * Commit the given Raft log.
     *
     * NOTE:
     *   Given memory Buffer is owned by caller, so that
     *   commit implementation should clone it if user wants to
     *   use the memory even after the commit call returns.
     *
     *   Here provide a default implementation for facilitating the
     *   situation when application does not care its implementation.
     *
     * @param log_idx Raft log number to commit.
     * @param data Payload of the Raft log.
     * @return Result value of state machine.
     */
    virtual ptr<Buffer> commit(const ulong log_idx,
                               Buffer& data) { return nullptr; }

    /**
     * (Optional)
     * Extended version of `commit`, for users want to keep
     * the data without any extra memory copy.
     */
    virtual ptr<Buffer> commit_ext(const ext_op_params& params)
    {   return commit(params.log_idx, *params.data);    }

    /**
     * (Optional)
     * Handler on the commit of a configuration change.
     *
     * @param log_idx Raft log number of the configuration change.
     * @param new_conf New cluster configuration.
     */
    virtual void commit_config(const ulong log_idx, ptr<ClusterConfig>& new_conf) { }

    /**
     * Pre-commit the given Raft log.
     *
     * Pre-commit is called after appending Raft log,
     * before getting acks from quorum nodes.
     * Users can ignore this function if not needed.
     *
     * Same as `commit()`, memory Buffer is owned by caller.
     *
     * @param log_idx Raft log number to commit.
     * @param data Payload of the Raft log.
     * @return Result value of state machine.
     */
    virtual ptr<Buffer> pre_commit(const ulong log_idx,
                                   Buffer& data) { return nullptr; }

    /**
     * (Optional)
     * Extended version of `pre_commit`, for users want to keep
     * the data without any extra memory copy.
     */
    virtual ptr<Buffer> pre_commit_ext(const ext_op_params& params)
    {   return pre_commit(params.log_idx, *params.data);  }

    /**
     * Rollback the state machine to given Raft log number.
     *
     * It will be called for uncommitted Raft logs only,
     * so that users can ignore this function if they don't
     * do anything on pre-commit.
     *
     * Same as `commit()`, memory Buffer is owned by caller.
     *
     * @param log_idx Raft log number to commit.
     * @param data Payload of the Raft log.
     */
    virtual void rollback(const ulong log_idx,
                          Buffer& data) {}

    /**
     * (Optional)
     * Handler on the rollback of a configuration change.
     * The configuration can be either committed or uncommitted one,
     * and that can be checked by the given `log_idx`, comparing it with
     * the current `ClusterConfig`'s log index.
     *
     * @param log_idx Raft log number of the configuration change.
     * @param conf The cluster configuration to be rolled back.
     */
    virtual void rollback_config(const ulong log_idx, ptr<ClusterConfig>& conf) { }

    /**
     * (Optional)
     * Extended version of `rollback`, for users want to keep
     * the data without any extra memory copy.
     */
    virtual void rollback_ext(const ext_op_params& params)
    {   rollback(params.log_idx, *params.data);  }

    /**
     * (Optional)
     * Return a hint about the preferred size (in number of bytes)
     * of the next batch of logs to be sent from the leader.
     *
     * Only applicable on followers.
     *
     * @return The preferred size of the next log batch.
     *         `0` indicates no preferred size (any size is good).
     *         `positive value` indicates at least one log can be sent,
     *         (the size of that log may be bigger than this hint size).
     *         `negative value` indicates no log should be sent since this
     *         follower is busy handling pending logs.
     */
    virtual int64 get_next_batch_size_hint_in_bytes() { return 0; }

    /**
     * (Deprecated)
     * Save the given Snapshot chunk to local Snapshot.
     * This API is for Snapshot receiver (i.e., follower).
     *
     * Since Snapshot itself may be quite big, save_Snapshot_data()
     * will be invoked multiple times for the same Snapshot `s`. This
     * function should decode the {offset, data} and re-construct the
     * Snapshot. After all savings are done, apply_Snapshot() will be
     * called at the end.
     *
     * Same as `commit()`, memory Buffer is owned by caller.
     *
     * @param s Snapshot instance to save.
     * @param offset Byte offset of given chunk.
     * @param data Payload of given chunk.
     */
    virtual void save_Snapshot_data(Snapshot& s,
                                    const ulong offset,
                                    Buffer& data) {}

    /**
     * Save the given Snapshot object to local Snapshot.
     * This API is for Snapshot receiver (i.e., follower).
     *
     * This is an optional API for users who want to use logical
     * Snapshot. Instead of splitting a Snapshot into multiple
     * physical chunks, this API uses logical objects corresponding
     * to a unique object ID. Users are responsible for defining
     * what object is: it can be a key-value pair, a set of
     * key-value pairs, or whatever.
     *
     * Same as `commit()`, memory Buffer is owned by caller.
     *
     * @param s Snapshot instance to save.
     * @param obj_id[in,out]
     *     Object ID.
     *     As a result of this API call, the next object ID
     *     that reciever wants to get should be set to
     *     this parameter.
     * @param data Payload of given object.
     * @param is_first_obj `true` if this is the first object.
     * @param is_last_obj `true` if this is the last object.
     */
    virtual void save_logical_snp_obj(Snapshot& s,
                                      ulong& obj_id,
                                      Buffer& data,
                                      bool is_first_obj,
                                      bool is_last_obj) {}

    /**
     * Apply received Snapshot to state machine.
     *
     * @param s Snapshot instance to apply.
     * @returm `true` on success.
     */
    virtual bool apply_Snapshot(Snapshot& s) = 0;

    /**
     * (Deprecated)
     * Read the given Snapshot chunk.
     * This API is for Snapshot sender (i.e., leader).
     *
     * @param s Snapshot instance to read.
     * @param offset Byte offset of given chunk.
     * @param[out] data Buffer where the read chunk will be stored.
     * @return Amount of bytes read.
     *         0 if failed.
     */
    virtual int read_Snapshot_data(Snapshot& s,
                                   const ulong offset,
                                   Buffer& data) { return 0; }

    /**
     * Read the given Snapshot object.
     * This API is for Snapshot sender (i.e., leader).
     *
     * Same as above, this is an optional API for users who want to
     * use logical Snapshot.
     *
     * @param s Snapshot instance to read.
     * @param[in,out] user_snp_ctx
     *     User-defined instance that needs to be passed through
     *     the entire Snapshot read. It can be a pointer to
     *     state machine specific iterators, or whatever.
     *     On the first `read_logical_snp_obj` call, it will be
     *     set to `null`, and this API may return a new pointer if necessary.
     *     Returned pointer will be passed to next `read_logical_snp_obj`
     *     call.
     * @param obj_id Object ID to read.
     * @param[out] data Buffer where the read object will be stored.
     * @param[out] is_last_obj Set `true` if this is the last object.
     * @return Negative number if failed.
     */
    virtual int read_logical_snp_obj(Snapshot& s,
                                     void*& user_snp_ctx,
                                     ulong obj_id,
                                     ptr<Buffer>& data_out,
                                     bool& is_last_obj) {
        data_out = Buffer::alloc(4); // A dummy Buffer.
        is_last_obj = true;
        return 0;
    }

    /**
     * Free user-defined instance that is allocated by
     * `read_logical_snp_obj`.
     * This is an optional API for users who want to use logical Snapshot.
     *
     * @param user_snp_ctx User-defined instance to free.
     */
    virtual void free_user_snp_ctx(void*& user_snp_ctx) {}

    /**
     * Get the latest Snapshot instance.
     *
     * This API will be invoked at the initialization of Raft server,
     * so that the last last Snapshot should be durable for server restart,
     * if you want to avoid unnecessary catch-up.
     *
     * @return Pointer to the latest Snapshot.
     */
    virtual ptr<Snapshot> last_Snapshot() = 0;

    /**
     * Get the last committed Raft log number.
     *
     * This API will be invoked at the initialization of Raft server
     * to identify what the last committed point is, so that the last
     * committed index number should be durable for server restart,
     * if you want to avoid unnecessary catch-up.
     *
     * @return Last committed Raft log number.
     */
    virtual ulong last_commit_index() = 0;

    /**
     * Create a Snapshot corresponding to the given info.
     *
     * @param s Snapshot info to create.
     * @param when_done Callback function that will be called after
     *                  Snapshot creation is done.
     */
    virtual void create_Snapshot(Snapshot& s,
                                 async_result<bool>::handler_type& when_done) = 0;

    /**
     * Decide to create Snapshot or not.
     * Once the pre-defined condition is satisfied, Raft core will invoke
     * this function to ask if it needs to create a new Snapshot.
     * If user-defined state machine does not want to create Snapshot
     * at this time, this function will return `false`.
     *
     * @return `true` if wants to create Snapshot.
     *         `false` if does not want to create Snapshot.
     */
    virtual bool chk_create_Snapshot() { return true; }

    /**
     * Decide to transfer leadership.
     * Once the other conditions are met, Raft core will invoke
     * this function to ask if it is allowed to transfer the
     * leadership to other member.
     *
     * @return `true` if wants to transfer leadership.
     *         `false` if not.
     */
    virtual bool allow_leadership_transfer() { return true; }

    /**
     * Parameters for `adjust_commit_index` API.
     */
    struct adjust_commit_index_params {
        adjust_commit_index_params()
            : current_commit_index_(0)
            , expected_commit_index_(0)
            {}

        /**
         * The current committed index.
         */
        uint64_t current_commit_index_;

        /**
         * The new target commit index determined by Raft.
         */
        uint64_t expected_commit_index_;

        /**
         * A map of <peer ID, peer's log index>, including the
         * leader and learners.
         */
        std::unordered_map<int, uint64_t> peer_index_map_;
    };

    /**
     * This function will be called when Raft succeeds in replicating logs
     * to an arbitrary follower and attempts to commit logs. Users can manually
     * adjust the commit index. The adjusted commit index should be equal to
     * or greater than the given `current_commit_index`. Otherwise, no log
     * will be committed.
     *
     * @param params Parameters.
     * @return Adjusted commit index.
     */
    virtual uint64_t adjust_commit_index(const adjust_commit_index_params& params) {
        return params.expected_commit_index_;
    }
};

}

#endif //_STATE_MACHINE_HXX_
