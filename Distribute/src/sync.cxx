#include "raft_server.hxx"

#include "context.hxx"
#include "error_code.hxx"
#include "Awaiter.hxx"
#include "peer.hxx"
#include "Snapshot.hxx"
#include "Snapshot_sync_ctx.hxx"
#include "state_machine.hxx"
#include "state_mgr.hxx"
#include "tracer.hxx"
#include "Snapshot_sync_req.hxx"

#include <cstring>
#include <cassert>
#include <sstream>

namespace SDN_Raft {

int32 raft_server::get_Snapshot_sync_block_size() const {
    int32 block_size = ctx_->get_params()->Snapshot_block_size_;
    return block_size == 0 ? default_Snapshot_sync_block_size : block_size;
}

bool raft_server::check_Snapshot_timeout(ptr<peer> pp) {
    ptr<Snapshot_sync_ctx> sync_ctx = pp->get_Snapshot_sync_ctx();
    if (!sync_ctx) return false;

    if ( sync_ctx->get_timer().timeout() ) {
        output_warn("Snapshot install task for peer %d timed out: %" PRIu64 " ms, "
             "reset Snapshot sync context %p",
             pp->get_id(), sync_ctx->get_timer().get_ms(), sync_ctx.get());
        clear_Snapshot_sync_ctx(*pp);
        return true;
    }
    return false;
}

void raft_server::destroy_user_snp_ctx(ptr<Snapshot_sync_ctx> sync_ctx) {
    if (!sync_ctx) return;
    void*& user_ctx = sync_ctx->get_user_snp_ctx();
    output_trace("destroy user ctx %p", user_ctx);
    state_machine_->free_user_snp_ctx(user_ctx);
}

void raft_server::clear_Snapshot_sync_ctx(peer& pp) {
    ptr<Snapshot_sync_ctx> snp_ctx = pp.get_Snapshot_sync_ctx();
    if (snp_ctx) {
        destroy_user_snp_ctx(snp_ctx);
        output_trace("destroy Snapshot sync ctx %p", snp_ctx.get());
    }
    pp.set_Snapshot_in_sync(nullptr);
}

ptr<RequestMessage> raft_server::create_sync_Snapshot_req(ptr<peer>& pp,
                                                   ulong last_log_idx,
                                                   ulong term,
                                                   ulong commit_idx,
                                                   bool& succeeded_out) {
    succeeded_out = false;
    peer& p = *pp;
    ptr<raft_params> params = ctx_->get_params();
    std::unique_lock<std::mutex> guard(p.get_lock());
    ptr<Snapshot_sync_ctx> sync_ctx = p.get_Snapshot_sync_ctx();
    ptr<Snapshot> snp = nullptr;
    ulong prev_sync_snp_log_idx = 0;
    if (sync_ctx) {
        snp = sync_ctx->get_Snapshot();
        output_debug( "previous sync_ctx exists %p, offset %" PRIu64 ", snp idx %" PRIu64
              ", user_ctx %p",
              sync_ctx.get(),
              sync_ctx->get_offset(),
              snp->get_last_log_idx(),
              sync_ctx->get_user_snp_ctx() );
        prev_sync_snp_log_idx = snp->get_last_log_idx();

        if (sync_ctx->get_timer().timeout()) {
            output_info("previous sync_ctx %p timed out, reset it", sync_ctx.get());
            destroy_user_snp_ctx(sync_ctx);
            sync_ctx.reset();
            snp.reset();
        }
    }

    // Modified by Jung-Sang Ahn, May 15 2018:
    //   Even though new Snapshot has been created,
    //   keep using existing Snapshot, as new Snapshot will reset
    //   previous catching-up.
    //
    // if ( !snp /*||
    //      ( last_Snapshot_ &&
    //        last_Snapshot_->get_last_log_idx() > snp->get_last_log_idx() )*/ ) {
    if ( !snp || sync_ctx->get_offset() == 0 ) {
        snp = get_last_Snapshot();
        if ( snp == nilptr ||
             last_log_idx > snp->get_last_log_idx() ) {
            // LCOV_EXCL_START
            p_er( "system is running into fatal errors, failed to find a "
                  "Snapshot for peer %d (Snapshot null: %d, Snapshot "
                  "doesn't contais lastLogIndex: %d)",
                  p.get_id(), snp == nilptr ? 1 : 0,
                  last_log_idx > snp->get_last_log_idx() ? 1 : 0 );
            if (snp) {
                p_er("last log idx %" PRIu64 ", snp last log idx %" PRIu64,
                     last_log_idx, snp->get_last_log_idx());
            }
            ctx_->state_mgr_->system_exit(raft_err::N16_Snapshot_for_peer_not_found);
            ::exit(-1);
            return ptr<RequestMessage>();
            // LCOV_EXCL_STOP
        }

        if ( snp->get_type() == Snapshot::raw_binary &&
             snp->size() < 1L ) {
            // LCOV_EXCL_START
            p_er("invalid Snapshot, this usually means a bug from state "
                 "machine implementation, stop the system to prevent "
                 "further errors");
            ctx_->state_mgr_->system_exit(raft_err::N17_empty_Snapshot);
            ::exit(-1);
            return ptr<RequestMessage>();
            // LCOV_EXCL_STOP
        }

        if (snp->get_last_log_idx() != prev_sync_snp_log_idx) {
            output_info( "trying to sync Snapshot with last index %" PRIu64 " to peer %d, "
                  "its last log idx %" PRIu64 "",
                  snp->get_last_log_idx(), p.get_id(), last_log_idx );
        }
        if (sync_ctx) {
            // If previous user context exists, should free it
            // as it causes memory leak.
            destroy_user_snp_ctx(sync_ctx);
        }

        // Timeout: heartbeat * response limit.
        ulong snp_timeout_ms = ctx_->get_params()->heart_beat_interval_ *
                               raft_server::raft_limits_.response_limit_;
        p.set_Snapshot_in_sync(snp, snp_timeout_ms);
    }

    if (params->use_bg_thread_for_Snapshot_io_) {
        // If async Snapshot IO, push the Snapshot read request to the manager
        // and immediately return here.
        Snapshot_io_mgr::instance().push( this->shared_from_this(),
                                          pp,
                                          ( (pp == srv_to_join_)
                                            ? ex_resp_handler_
                                            : resp_handler_ ) );
        succeeded_out = true;
        return nullptr;
    }
    // Otherwise (sync Snapshot IO), read the requested object here and then return.

    bool last_request = false;
    ptr<Buffer> data = nullptr;
    ulong data_idx = 0;
    if (snp->get_type() == Snapshot::raw_binary) {
        // LCOV_EXCL_START
        // Raw binary Snapshot (original)
        ulong offset = p.get_Snapshot_sync_ctx()->get_offset();
        ulong sz_left = ( snp->size() > offset ) ? ( snp->size() - offset ) : 0;
        int32 blk_sz = get_Snapshot_sync_block_size();
        data = Buffer::alloc((size_t)(std::min((ulong)blk_sz, sz_left)));
        int32 sz_rd = state_machine_->read_Snapshot_data(*snp, offset, *data);
        if ((size_t)sz_rd < data->size()) {
            // LCOV_EXCL_START
            p_er( "only %d bytes could be read from Snapshot while %zu "
                  "bytes are expected, must be something wrong, exit.",
                  sz_rd, data->size() );
            ctx_->state_mgr_->system_exit(raft_err::N18_partial_Snapshot_block);
            ::exit(-1);
            return ptr<RequestMessage>();
            // LCOV_EXCL_STOP
        }
        last_request = (offset + (ulong)data->size()) >= snp->size();
        data_idx = offset;
        // LCOV_EXCL_STOP

    } else {
        // Logical object type Snapshot
        sync_ctx = p.get_Snapshot_sync_ctx();
        ulong obj_idx = sync_ctx->get_offset();
        void*& user_snp_ctx = sync_ctx->get_user_snp_ctx();
        output_verbose("peer: %d, obj_idx: %" PRIu64 ", user_snp_ctx %p",
             (int)p.get_id(), obj_idx, user_snp_ctx);

        int rc = state_machine_->read_logical_snp_obj( *snp, user_snp_ctx, obj_idx,
                                                       data, last_request );
        if (rc < 0) {
            output_warn( "reading Snapshot (idx %" PRIu64 ", term %" PRIu64
                  ", object %" PRIu64 ") failed: %d",
                  snp->get_last_log_idx(),
                  snp->get_last_log_term(),
                  obj_idx,
                  rc );
            // Reset the `sync_ctx` so as to retry with the newer version.
            clear_Snapshot_sync_ctx(p);
            return nullptr;
        }
        if (data) data->pos(0);
        data_idx = obj_idx;
    }

    std::unique_ptr<Snapshot_sync_req> sync_req
        ( new Snapshot_sync_req(snp, data_idx, data, last_request) );
    ptr<RequestMessage> req( cs_new<RequestMessage>
                      ( term,
                        MessageType::install_Snapshot_request,
                        id_,
                        p.get_id(),
                        snp->get_last_log_term(),
                        snp->get_last_log_idx(),
                        commit_idx ) );
    req->log_entries().push_back( cs_new<LogEntry>
                                  ( term,
                                    sync_req->serialize(),
                                    LogType::snp_sync_req ) );

    succeeded_out = true;
    return req;
}

ptr<resp_msg> raft_server::handle_install_Snapshot_req(RequestMessage& req, std::unique_lock<std::recursive_mutex>& guard) {
    if (req.get_term() == state_->get_term() && !catching_up_) {
        if (role_ == srv_role::candidate) {
            become_follower();

        } else if (role_ == srv_role::leader) {
            // LCOV_EXCL_START
            p_er( "Receive InstallSnapshotRequest from another leader(%d) "
                  "with same term, there must be a bug, server exits",
                  req.get_src() );
            ctx_->state_mgr_->system_exit
                ( raft_err::N10_leader_receive_InstallSnapshotRequest );
            ::exit(-1);
            return ptr<resp_msg>();
            // LCOV_EXCL_STOP

        } else {
            restart_election_timer();
        }
    }

    ptr<resp_msg> resp = cs_new<resp_msg>
                         ( state_->get_term(),
                           MessageType::install_Snapshot_response,
                           id_,
                           req.get_src(),
                           LogStore_->next_slot() );

    if (!catching_up_ && req.get_term() < state_->get_term()) {
        output_warn("received an install Snapshot request (%" PRIu64 ") which has lower term "
             "than this server (%" PRIu64 "), decline the request",
             req.get_term(), state_->get_term());
        return resp;
    }

    std::vector<ptr<LogEntry>>& entries(req.log_entries());
    if ( entries.size() != 1 ||
         entries[0]->get_val_type() != LogType::snp_sync_req ) {
        output_warn("Receive an invalid InstallSnapshotRequest due to "
             "bad log entries or bad log entry value");
        return resp;
    }

    ptr<Snapshot_sync_req> sync_req =
        Snapshot_sync_req::deserialize(entries[0]->get_buf());
    if (sync_req->get_Snapshot().get_last_log_idx() <= quick_commit_index_) {
        output_warn( "received a Snapshot (%" PRIu64 ") that is older than "
              "current commit idx (%" PRIu64 "), last log idx %" PRIu64,
              sync_req->get_Snapshot().get_last_log_idx(),
              quick_commit_index_.load(),
              LogStore_->next_slot() - 1);
        // Put dummy CTX to end the Snapshot sync.
        ptr<Buffer> done_ctx = Buffer::alloc(1);
        done_ctx->pos(0);
        done_ctx->put((byte)0);
        done_ctx->pos(0);
        resp->set_ctx(done_ctx);
        return resp;
    }

    if (handle_Snapshot_sync_req(*sync_req, guard)) {
        if (sync_req->get_Snapshot().get_type() == Snapshot::raw_binary) {
            // LCOV_EXCL_START
            // Raw binary: add received byte to offset.
            resp->accept(sync_req->get_offset() + sync_req->get_data().size());
            // LCOV_EXCL_STOP

        } else {
            // Object type: add one (next object index).
            resp->accept(sync_req->get_offset());
            if (sync_req->is_done()) {
                // TODO: check if there is missing object.
                // Add a context Buffer to inform installation is done.
                ptr<Buffer> done_ctx = Buffer::alloc(1);
                done_ctx->pos(0);
                done_ctx->put((byte)0);
                done_ctx->pos(0);
                resp->set_ctx(done_ctx);
            }
        }
    }

    return resp;
}

void raft_server::handle_install_Snapshot_resp(resp_msg& resp) {
    output_debug("%s\n", resp.get_accepted() ? "accepted" : "not accepted");
    peer_itor it = peers_.find(resp.get_src());
    if (it == peers_.end()) {
        output_info("the response is from an unknown peer %d", resp.get_src());
        return;
    }

    // if there are pending logs to be synced or commit index need to be advanced,
    // continue to send appendEntries to this peer
    bool need_to_catchup = true;
    ptr<peer> p = it->second;
    if (resp.get_accepted()) {
        std::lock_guard<std::mutex> guard(p->get_lock());
        ptr<Snapshot_sync_ctx> sync_ctx = p->get_Snapshot_sync_ctx();
        if (sync_ctx == nullptr) {
            output_info("no Snapshot sync context for this peer, drop the response");
            need_to_catchup = false;

        } else {
            ptr<Snapshot> snp = sync_ctx->get_Snapshot();
            if (snp->get_type() == Snapshot::raw_binary) {
                // LCOV_EXCL_START
                output_debug("resp.get_next_idx(): %" PRIu64 ", snp->size(): %" PRIu64,
                     resp.get_next_idx(), snp->size());
                // LCOV_EXCL_STOP
            }

            bool snoutput_infostall_done =
                 ( snp->get_type() == Snapshot::raw_binary &&
                   resp.get_next_idx() >= snp->size() )           ||
                 ( snp->get_type() == Snapshot::logical_object &&
                   resp.get_ctx() );

            if (snoutput_infostall_done) {
                output_debug("Snapshot sync is done (raw type)");
                p->set_next_log_idx(sync_ctx->get_Snapshot()->get_last_log_idx() + 1);
                p->set_matched_idx(sync_ctx->get_Snapshot()->get_last_log_idx());
                clear_Snapshot_sync_ctx(*p);

                need_to_catchup = p->clear_pending_commit() ||
                                  p->get_next_log_idx() < LogStore_->next_slot();
                output_info("Snapshot done %" PRIu64 ", %" PRIu64 ", %d",
                     p->get_next_log_idx(), p->get_matched_idx(), need_to_catchup);
            } else {
                output_debug("continue to sync Snapshot at offset %" PRIu64,
                     resp.get_next_idx());
                sync_ctx->set_offset(resp.get_next_idx());
            }
        }

    } else {
        output_warn( "peer %d declined Snapshot: p->get_next_log_idx(): %" PRIu64 ", "
              "LogStore_->next_slot(): %" PRIu64,
              p->get_id(), p->get_next_log_idx(), LogStore_->next_slot() );
        p->set_next_log_idx(resp.get_next_idx());

        // Added by Jung-Sang Ahn (Oct 11 2017)
        // Declining Snapshot implies that the peer already has the up-to-date Snapshot.
        need_to_catchup = p->get_next_log_idx() < LogStore_->next_slot();

        // Should reset current Snapshot context,
        // to continue with more recent Snapshot.
        std::lock_guard<std::mutex> guard(p->get_lock());
        clear_Snapshot_sync_ctx(*p);
    }

    // This may not be a leader anymore, such as
    // the response was sent out long time ago
    // and the role was updated by UpdateTerm call
    // Try to match up the logs for this peer
    if (role_ == srv_role::leader && need_to_catchup) {
        request_append_entries(p);
    }
}

void raft_server::handle_install_Snapshot_resp_new_member(resp_msg& resp) {
    if (!srv_to_join_) {
        output_info("no server to join, the response must be very old.");
        return;
    }

    if (!resp.get_accepted()) {
        output_warn("peer doesn't accept the Snapshot installation request, "
             "next log idx %" PRIu64 ", "
             "but we can move forward",
             resp.get_next_idx());
        srv_to_join_->set_next_log_idx(resp.get_next_idx());
    }
    srv_to_join_->reset_resp_timer();

    ptr<Snapshot_sync_ctx> sync_ctx = srv_to_join_->get_Snapshot_sync_ctx();
    if (sync_ctx == nullptr) {
        output_fetal("SnapshotSyncContext must not be null: "
             "src %d dst %d my id %d leader id %d, "
             "maybe leader election happened in the meantime. "
             "next heartbeat or append request will cover it up.",
             resp.get_src(), resp.get_dst(), id_, leader_.load());
        return;
    }

    ptr<Snapshot> snp = sync_ctx->get_Snapshot();
    bool snoutput_infostall_done =
        ( snp->get_type() == Snapshot::raw_binary &&
          resp.get_next_idx() >= snp->size() )         ||
        ( snp->get_type() == Snapshot::logical_object &&
          resp.get_ctx() );

    if (snoutput_infostall_done) {
        // Snapshot is done
        output_info("Snapshot install is done\n");
        srv_to_join_->set_next_log_idx
            ( sync_ctx->get_Snapshot()->get_last_log_idx() + 1 );
        srv_to_join_->set_matched_idx
            ( sync_ctx->get_Snapshot()->get_last_log_idx() );

        clear_Snapshot_sync_ctx(*srv_to_join_);

        output_info( "Snapshot has been copied and applied to new server, "
              "continue to sync logs after Snapshot, "
              "next log idx %" PRIu64 ", matched idx %" PRIu64 "",
              srv_to_join_->get_next_log_idx(),
              srv_to_join_->get_matched_idx() );
    } else {
        sync_ctx->set_offset(resp.get_next_idx());
        output_debug( "continue to send Snapshot to new server at offset %" PRIu64 "",
              resp.get_next_idx() );
    }

    sync_log_to_new_srv(srv_to_join_->get_next_log_idx());
}

bool raft_server::handle_Snapshot_sync_req(Snapshot_sync_req& req, std::unique_lock<std::recursive_mutex>& guard) {
 try {
    // if offset == 0, it is the first object.
    bool is_first_obj = (req.get_offset()) ? false : true;
    bool is_last_obj = req.is_done();
    if (is_first_obj || is_last_obj) {
        // INFO level: log only first and last object.
        output_info("save Snapshot (idx %" PRIu64 ", term %" PRIu64 ") offset 0x%" PRIx64
             ", %s %s",
             req.get_Snapshot().get_last_log_idx(),
             req.get_Snapshot().get_last_log_term(),
             req.get_offset(),
             (is_first_obj) ? "first obj" : "",
             (is_last_obj)  ? "last obj"  : "" );
    } else {
        // above DEBUG: log all.
        output_debug("save Snapshot (idx %" PRIu64 ", term %" PRIu64 ") offset 0x%" PRIx64
             ", %s %s",
             req.get_Snapshot().get_last_log_idx(),
             req.get_Snapshot().get_last_log_term(),
             req.get_offset(),
             (is_first_obj) ? "first obj" : "",
             (is_last_obj)  ? "last obj"  : "" );
    }

    cb_func::Param param(id_, leader_);
    param.ctx = &req;
    CbReturnCode rc = ctx_->cb_func_.call(cb_func::SaveSnapshot, &param);
    if (rc == CbReturnCode::ReturnNull) {
        output_warn("by callback, return false");
        return false;
    }

    // Set flag to avoid initiating election by this node.
    receiving_Snapshot_ = true;
    et_cnt_receiving_Snapshot_ = 0;

    // Set initialized flag
    if (!initialized_) initialized_ = true;

    if (req.get_Snapshot().get_type() == Snapshot::raw_binary) {
        // LCOV_EXCL_START
        // Raw binary type (original).
        state_machine_->save_Snapshot_data(req.get_Snapshot(),
                                           req.get_offset(),
                                           req.get_data());
        // LCOV_EXCL_STOP

    } else {
        // Logical object type.
        ulong obj_id = req.get_offset();
        Buffer& buf = req.get_data();
        buf.pos(0);
        state_machine_->save_logical_snp_obj(req.get_Snapshot(),
                                             obj_id,
                                             buf,
                                             is_first_obj,
                                             is_last_obj);
        req.set_offset(obj_id);
    }

    if (is_last_obj) {
        // let's pause committing in backgroud so it doesn't access logs
        // while they are being compacted
        guard.unlock();
        pause_state_machine_exeuction();
        size_t wait_count = 0;
        while (!wait_for_state_machine_pause(500)) {
            output_info("waiting for state machine pause before applying Snapshot: count %zu",
                 ++wait_count);
        }
        guard.lock();

        struct ExecAutoResume {
            explicit ExecAutoResume(std::function<void()> func) : clean_func_(func) {}
            ~ExecAutoResume() { clean_func_(); }
            std::function<void()> clean_func_;
        } exec_auto_resume([this](){ resume_state_machine_execution(); });


        receiving_Snapshot_ = false;

        // Only follower will run this piece of code, but let's check it again
        if (role_ != srv_role::follower) {
            // LCOV_EXCL_START
            p_er("bad server role for applying a Snapshot, exit for debugging");
            ctx_->state_mgr_->system_exit(raft_err::N11_not_follower_for_Snapshot);
            ::exit(-1);
            // LCOV_EXCL_STOP
        }

        output_info( "successfully receive a Snapshot (idx %" PRIu64 " term %" PRIu64
              ") from leader",
              req.get_Snapshot().get_last_log_idx(),
              req.get_Snapshot().get_last_log_term() );
        if (LogStore_->compact(req.get_Snapshot().get_last_log_idx())) {
            // The state machine will not be able to commit anything before the
            // Snapshot is applied, so make this synchronously with election
            // timer stopped as usually applying a Snapshot may take a very
            // long time
            stop_election_timer();
            output_info("successfully compact the log store, will now ask the "
                 "statemachine to apply the Snapshot");
            if (!state_machine_->apply_Snapshot(req.get_Snapshot())) {
                // LCOV_EXCL_START
                p_er("failed to apply the Snapshot after log compacted, "
                     "to ensure the safety, will shutdown the system");
                ctx_->state_mgr_->system_exit(raft_err::N12_apply_Snapshot_failed);
                ::exit(-1);
                return false;
                // LCOV_EXCL_STOP
            }

            reconfigure(req.get_Snapshot().get_last_config());

            ptr<ClusterConfig> c_conf = get_config();
            ctx_->state_mgr_->save_config(*c_conf);

            precommit_index_ = req.get_Snapshot().get_last_log_idx();
            sm_commit_index_ = req.get_Snapshot().get_last_log_idx();
            quick_commit_index_ = req.get_Snapshot().get_last_log_idx();
            lagging_sm_target_index_ = req.get_Snapshot().get_last_log_idx();

            ctx_->state_mgr_->save_state(*state_);

            ptr<Snapshot> new_snp = cs_new<Snapshot>
                                    ( req.get_Snapshot().get_last_log_idx(),
                                      req.get_Snapshot().get_last_log_term(),
                                      c_conf,
                                      req.get_Snapshot().size(),
                                      req.get_Snapshot().get_type() );
            set_last_Snapshot(new_snp);

            restart_election_timer();
            output_info("Snapshot idx %" PRIu64 " term %" PRIu64 " is successfully applied, "
                 "log start %" PRIu64 " last idx %" PRIu64,
                 new_snp->get_last_log_idx(),
                 new_snp->get_last_log_term(),
                 LogStore_->start_index(),
                 LogStore_->next_slot() - 1);

        } else {
            p_er("failed to compact the log store after a Snapshot is received, "
                 "will ask the leader to retry");
            return false;
        }
    }

 } catch (...) {
    // LCOV_EXCL_START
    p_er("failed to handle Snapshot installation due to system errors");
    ctx_->state_mgr_->system_exit(raft_err::N13_Snapshot_install_failed);
    ::exit(-1);
    return false;
    // LCOV_EXCL_STOP
 }

    return true;
}

class raft_server;

Snapshot_sync_ctx::Snapshot_sync_ctx(const ptr<Snapshot>& s,
                                     int peer_id,
                                     ulong timeout_ms,
                                     ulong offset)
    : peer_id_(peer_id)
    , Snapshot_(s)
    , offset_(offset)
    , user_snp_ctx_(nullptr)
{
    // 10 seconds by default.
    timer_.set_duration_ms(timeout_ms);
}

void Snapshot_sync_ctx::set_offset(ulong offset) {
    if (offset_ != offset) timer_.reset();
    offset_ = offset;
}

struct Snapshot_io_mgr::io_queue_elem {
    io_queue_elem( ptr<raft_server> r,
                   ptr<Snapshot> s,
                   ptr<Snapshot_sync_ctx> c,
                   ptr<peer> p,
                   std::function< void(ptr<resp_msg>&, ptr<rpc_exception>&) >& h )
        : raft_(r)
        , Snapshot_(s)
        , sync_ctx_(c)
        , dst_(p)
        , handler_(h)
        {}
    ptr<raft_server> raft_;
    ptr<Snapshot> Snapshot_;
    ptr<Snapshot_sync_ctx> sync_ctx_;
    ptr<peer> dst_;
    std::function< void(ptr<resp_msg>&, ptr<rpc_exception>&) > handler_;
};


Snapshot_io_mgr::Snapshot_io_mgr()
    : io_thread_ea_(new EventAwaiter())
    , terminating_(false)
{
    io_thread_ = std::thread(&Snapshot_io_mgr::async_io_loop, this);
}

Snapshot_io_mgr::~Snapshot_io_mgr() {
    shutdown();
}

bool Snapshot_io_mgr::push(ptr<Snapshot_io_mgr::io_queue_elem>& elem) {
    auto_lock(queue_lock_);
    Logger* l_ = elem->raft_->l_.get();

    // If there is existing one for the same peer, ignore it.
    for (auto& entry: queue_) {
        if ( entry->raft_ == elem->raft_ &&
             entry->dst_->get_id() == elem->dst_->get_id() ) {
            output_trace("Snapshot request for peer %d already exists, do nothing",
                 elem->dst_->get_id());
            return false;
        }
    }
    queue_.push_back(elem);
    output_trace("added Snapshot request for peer %d", elem->dst_->get_id());

    return true;
}

bool Snapshot_io_mgr::push(ptr<raft_server> r,
                           ptr<peer> p,
                           std::function< void(ptr<resp_msg>&, ptr<rpc_exception>&) >& h)
{
    ptr<io_queue_elem> elem =
        cs_new<io_queue_elem>( r,
                               p->get_Snapshot_sync_ctx()->get_Snapshot(),
                               p->get_Snapshot_sync_ctx(),
                               p,
                               h );
    return push(elem);
}

void Snapshot_io_mgr::invoke() {
    io_thread_ea_->invoke();
}

void Snapshot_io_mgr::drop_reqs(raft_server* r) {
    auto_lock(queue_lock_);
    Logger* l_ = r->l_.get();
    auto entry = queue_.begin();
    while (entry != queue_.end()) {
        if ((*entry)->raft_.get() == r) {
            output_trace("drop Snapshot request for peer %d, raft server %p",
                 (*entry)->dst_->get_id(), r);
            entry = queue_.erase(entry);
        } else {
            entry++;
        }
    }
}

bool Snapshot_io_mgr::has_pending_request(raft_server* r, int srv_id) {
    auto_lock(queue_lock_);
    for (auto& entry: queue_) {
        if ( entry->raft_.get() == r &&
             entry->dst_->get_id() == srv_id ) {
            return true;
        }
    }
    return false;
}

void Snapshot_io_mgr::shutdown() {
    terminating_ = true;
    if (io_thread_.joinable()) {
        io_thread_ea_->invoke();
        io_thread_.join();
    }
}

void Snapshot_io_mgr::async_io_loop() {
    std::string thread_name = "SDN_Raft_snp_io";
#ifdef __linux__
    pthread_setname_np(pthread_self(), thread_name.c_str());
#elif __APPLE__
    pthread_setname_np(thread_name.c_str());
#endif

    do {
        io_thread_ea_->wait_ms(1000);
        io_thread_ea_->reset();

        std::list< ptr<io_queue_elem> > reqs;
        std::list< ptr<io_queue_elem> > reqs_to_return;
        if (!terminating_) {
            auto_lock(queue_lock_);
            reqs = queue_;
        }

        for (ptr<io_queue_elem>& elem: reqs) {
            if (terminating_) {
                break;
            }
            if (!elem->raft_->is_leader()) {
                break;
            }

            int dst_id = elem->dst_->get_id();

            std::unique_lock<std::mutex> lock(elem->dst_->get_lock());
            // ---- lock acquired
            Logger* l_ = elem->raft_->l_.get();
            ulong obj_idx = elem->sync_ctx_->get_offset();
            void*& user_snp_ctx = elem->sync_ctx_->get_user_snp_ctx();
            output_debug("peer: %d, obj_idx: %" PRIu64 ", user_snp_ctx %p",
                 dst_id, obj_idx, user_snp_ctx);

            ulong snp_log_idx = elem->Snapshot_->get_last_log_idx();
            ulong snp_log_term = elem->Snapshot_->get_last_log_term();
            // ---- lock released
            lock.unlock();

            ptr<Buffer> data = nullptr;
            bool is_last_request = false;

            int rc = elem->raft_->state_machine_->read_logical_snp_obj
                     ( *elem->Snapshot_, user_snp_ctx, obj_idx,
                       data, is_last_request );
            if (rc < 0) {
                // Snapshot read failed.
                output_warn( "reading Snapshot (idx %" PRIu64 ", term %" PRIu64
                      ", object %" PRIu64 ") "
                      "for peer %d failed: %d",
                      snp_log_idx, snp_log_term, obj_idx, dst_id, rc );

                recur_lock(elem->raft_->lock_);
                auto entry = elem->raft_->peers_.find(dst_id);
                if (entry != elem->raft_->peers_.end()) {
                    // If normal member (already in the peer list):
                    //   reset the `sync_ctx` so as to retry with the newer version.
                    elem->raft_->clear_Snapshot_sync_ctx(*elem->dst_);
                } else {
                    // If it is joing the server (not in the peer list),
                    // enable HB temporarily to retry the request.
                    elem->raft_->srv_to_join_snp_retry_required_ = true;
                    elem->raft_->enable_hb_for_peer(*elem->raft_->srv_to_join_);
                }

                continue;
            }
            if (data) data->pos(0);

            // Send Snapshot message with the given response handler.
            recur_lock(elem->raft_->lock_);
            ulong term = elem->raft_->state_->get_term();
            ulong commit_idx = elem->raft_->quick_commit_index_;

            std::unique_ptr<Snapshot_sync_req> sync_req(
                new Snapshot_sync_req( elem->Snapshot_, obj_idx,
                                       data, is_last_request ) );
            ptr<RequestMessage> req( cs_new<RequestMessage>
                              ( term,
                                MessageType::install_Snapshot_request,
                                elem->raft_->id_,
                                dst_id,
                                elem->Snapshot_->get_last_log_term(),
                                elem->Snapshot_->get_last_log_idx(),
                                commit_idx ) );
            req->log_entries().push_back( cs_new<LogEntry>
                                          ( term,
                                            sync_req->serialize(),
                                            LogType::snp_sync_req ) );
            if (elem->dst_->make_busy()) {
                elem->dst_->set_rsv_msg(nullptr, nullptr);
                elem->dst_->send_req(elem->dst_, req, elem->handler_);
                elem->dst_->reset_ls_timer();
                output_trace("bg thread sent message to peer %d", dst_id);

            } else {
                output_debug("peer %d is busy, push the request back to queue", dst_id);
                reqs_to_return.push_back(elem);
            }
        }

        {
            auto_lock(queue_lock_);
            // Remove elements in `reqs` from `queue_`.
            for (auto& entry: reqs) {
                auto e2 = queue_.begin();
                while (e2 != queue_.end()) {
                    if (*e2 == entry) {
                        e2 = queue_.erase(e2);
                        break;
                    } else {
                        e2++;
                    }
                }
            }
            // Return elements in `reqs_to_return` to `queue_` for retrying.
            for (auto& entry: reqs_to_return) {
                queue_.push_back(entry);
            }
        }

    } while (!terminating_);
}

ptr<Snapshot_sync_req> Snapshot_sync_req::deserialize(Buffer& buf) {
    BufferSerializer bs(buf);
    return deserialize(bs);
}

ptr<Snapshot_sync_req> Snapshot_sync_req::deserialize(BufferSerializer& bs) {
    ptr<Snapshot> snp(Snapshot::deserialize(bs));
    ulong offset = bs.get_u64();
    bool done = bs.get_u8() == 1;
    byte* src = (byte*)bs.data();
    ptr<Buffer> b;
    if (bs.pos() < bs.size()) {
        size_t sz = bs.size() - bs.pos();
        b = Buffer::alloc(sz);
        ::memcpy(b->data(), src, sz);
    }
    else {
        b = Buffer::alloc(0);
    }

    return cs_new<Snapshot_sync_req>(snp, offset, b, done);
}

ptr<Buffer> Snapshot_sync_req::serialize() {
    ptr<Buffer> snp_buf = Snapshot_->serialize();
    ptr<Buffer> buf = Buffer::alloc(snp_buf->size() + sz_ulong + sz_byte + (data_->size() - data_->pos()));
    buf->put(*snp_buf);
    buf->put(offset_);
    buf->put(done_ ? (byte)1 : (byte)0);
    buf->put(*data_);
    buf->pos(0);
    return buf;
}

} 

