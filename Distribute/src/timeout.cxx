#include "raft_server.hxx"

#include "Awaiter.hxx"
#include "peer.hxx"
#include "state_machine.hxx"
#include "state_mgr.hxx"
#include "tracer.hxx"

#include <cassert>
#include <sstream>

namespace SDN_Raft {

void raft_server::enable_hb_for_peer(peer& p) {
    p.enable_hb(true);
    p.resume_hb_speed();
    output_trace("peer %d, interval: %d\n", p.get_id(), p.get_current_hb_interval());
    schedule_task(p.get_hb_task(), p.get_current_hb_interval());
}

void raft_server::check_srv_to_leave_timeout() {
    if (!srv_to_leave_) return;
    ulong last_resp_ms = srv_to_leave_->get_resp_timer_us() / 1000;
    if ( last_resp_ms >
             (ulong)raft_server::raft_limits_.leave_limit_ *
             ctx_->get_params()->heart_beat_interval_ ) {
        // Timeout: remove peer.
        output_warn("server to be removed %d, response timeout %" PRIu64 " ms. "
             "force remove now",
             srv_to_leave_->get_id(),
             last_resp_ms);
        remove_peer_from_peers(srv_to_leave_);
        reset_srv_to_leave();
    }
}

void raft_server::handle_hb_timeout(int32 srv_id) {
    recur_lock(lock_);

    check_srv_to_leave_timeout();

    if (write_paused_ && reelection_timer_.timeout()) {
        output_info("resign by timeout, %" PRIu64 " us elapsed, resign now",
             reelection_timer_.get_us());
        leader_ = -1;
        become_follower();

        // Clear this flag to avoid pre-vote rejection.
        hb_alive_ = false;
        return;
    }

    if ( srv_to_join_snp_retry_required_ &&
         srv_to_join_ &&
         srv_to_join_->get_id() == srv_id ) {
        output_info("retrying Snapshot read for server %d", srv_id);
        if (srv_to_join_->need_to_reconnect()) {
            output_info("rpc client for %d needs reconnection", srv_id);

            ptr<raft_params> params = ctx_->get_params();
            uint64_t resp_timer_ms = srv_to_join_->get_resp_timer_us() / 1000;
            if ( resp_timer_ms >= (uint64_t)params->heart_beat_interval_ *
                                  raft_server::raft_limits_.response_limit_ ) {
                output_info("response timeout: %" PRIu64 " ms, will not retry", resp_timer_ms);
                clear_Snapshot_sync_ctx(*srv_to_join_);
                return;
            }

            ptr<srv_config> s_config =
                srv_config::deserialize( *srv_to_join_->get_config().serialize() );
            bool succ = srv_to_join_->recreate_rpc(s_config, *ctx_);
            if (!succ) {
                // Reconnection failed.
                output_warn("reconnection failed, will not retry");
                clear_Snapshot_sync_ctx(*srv_to_join_);
                return;
            }
        }
        sync_log_to_new_srv(0);
        return;
    }

    auto pit = peers_.find(srv_id);
    if (pit == peers_.end()) {
        p_er("heartbeat handler error: server %d not exist", srv_id);
        return;
    }

    // To avoid freeing this pointer in the middle of this function.
    ptr<peer> p = pit->second;

    if (p->is_leave_flag_set()) {
        // Leave request has been sent but not removed yet,
        // increase the counter.
        p->inc_hb_cnt_since_leave();
        int32 cur_cnt = p->get_hb_cnt_since_leave();
        output_info("peer %d is not responding for %d HBs since leave request",
             p->get_id(), cur_cnt);

        if (cur_cnt >= raft_server::raft_limits_.leave_limit_) {
            // Force remove the server.
            p_er("force remove peer %d", p->get_id());
            handle_join_leave_rpc_err(MessageType::leave_cluster_request, p);
            return;
        }
    }

    cb_func::Param param(id_, leader_, p->get_id());
    uint64_t last_log_idx = LogStore_->next_slot() - 1;
    param.ctx = &last_log_idx;
    CbReturnCode rc = ctx_->cb_func_.call(cb_func::HeartBeat, &param);
    (void)rc;

    // Server is being shut down.
    if (stopping_) {
        output_warn("Triggered HB timer but server is shutting down");
        return;
    }

    if (!check_leadership_validity()) return;

    output_debug("heartbeat timeout for %d", p->get_id());
    if (role_ == srv_role::leader) {
        update_target_priority();
        request_append_entries(p);
        {
            std::lock_guard<std::mutex> guard(p->get_lock());
            if (p->is_hb_enabled()) {
                // Schedule another heartbeat if heartbeat is still enabled
                schedule_task(p->get_hb_task(), p->get_current_hb_interval());
                output_trace("reschedule heartbeat for peer %d", p->get_id());
            } else {
                output_debug("heartbeat is disabled for peer %d", p->get_id());
            }
        }
    } else {
        output_warn("Receive a heartbeat event for %d "
             "while no longer as a leader", p->get_id());
    }
}

void raft_server::restart_election_timer() {
    // don't start the election timer while this server is still catching up the logs
    // or this server is the leader
    recur_lock(lock_);
    if (catching_up_ || role_ == srv_role::leader) {
        return;
    }

    // If election timer was not allowed, clear the flag.
    if (!state_->is_election_timer_allowed()) {
        state_->allow_election_timer(true);
        ctx_->state_mgr_->save_state(*state_);
    }

    if (election_task_) {
        output_trace("cancel existing timer");
        cancel_task(election_task_);
    } else {
        election_task_ = cs_new< timer_task<void> >
                               ( election_exec_,
                                 timer_task_type::election_timer );
    }

    output_trace("re-schedule election timer");
    last_election_timer_reset_.reset();

    schedule_task(election_task_, rand_timeout_());
}

void raft_server::stop_election_timer() {
    if (!election_task_) {
        output_warn("Election Timer is never started but is "
             "requested to stop, protential a bug");
        return;
    }

    cancel_task(election_task_);
}

void raft_server::handle_election_timeout() {
    output_trace("election timeout");
    recur_lock(lock_);
    if (stopping_) {
        output_warn("Triggered election timer but server is shutting down");
        return;
    }

    if (steps_to_down_ > 0) {
        if (--steps_to_down_ == 0) {
            output_info("no hearing further news from leader, "
                 "remove this server from cluster and step down");
            // Modified by Jung-Sang Ahn (Oct 25, 2017):
            // Should maintain the info of itself in the config,
            // for the next launch.
            /*
            for ( std::list<ptr<srv_config>>::iterator it =
                      config_->get_servers().begin();
                  it != config_->get_servers().end();
                  ++it ) {
                if ((*it)->get_id() == id_) {
                    config_->get_servers().erase(it);
                    ctx_->state_mgr_->save_config(*config_);
                    break;
                }
            }
            */
            state_->allow_election_timer(false);
            ctx_->state_mgr_->save_state(*state_);

            // Modified by Jung-Sang Ahn (Dec 24, 2019):
            // Same as in reconfigure().
            //reset_peer_info();
            cancel_schedulers();
            return;
        }

        output_info( "stepping down (cycles left: %d), "
              "skip this election timeout event",
              steps_to_down_ );
        restart_election_timer();
        return;
    }

    if (catching_up_) {
        // this is a new server for the cluster, will not send out vote req
        // until conf that includes this srv is committed
        output_info("election timeout while joining the cluster, ignore it.");
        restart_election_timer();
        return;
    }

    if (out_of_log_range_) {
        output_warn("Triggered election timer but server is out of log range");
        return;
    }

    if (receiving_Snapshot_ && et_cnt_receiving_Snapshot_ < 20) {
        // If this node is receiving Snapshot,
        // ignore election timeout 20 times.
        et_cnt_receiving_Snapshot_.fetch_add(1);
        output_warn("election timeout while receiving Snapshot, count %" PRIu64 ", "
             "ignore it.", et_cnt_receiving_Snapshot_.load());
        restart_election_timer();
        return;
    }

    int time_ms = last_election_timer_reset_.get_us() / 1000;
    if ( serving_req_ ||
         time_ms < ctx_->get_params()->election_timeout_lower_bound_ ) {
        // Handling appending entries is now taking long time,
        // so that server keeps skipping sending heartbeat.
        // It doesn't mean server is gone. Just ignore.
        output_info("election timeout while serving append entries, ignore it.");
        restart_election_timer();
        return;
    }

    if (role_ == srv_role::leader) {
        p_er( "A leader should never encounter election timeout, "
              "illegal application state, ignore it.");
        return;
    }

    // Only voting member can suggest vote.
    if (!im_learner_) {
        output_warn("Election timeout, initiate leader election");
        if (!hb_alive_) {
            // Not the first election timeout, decay the target priority.
            decay_target_priority();
        }

        ulong last_log_term = 0;
        if (LogStore_ && LogStore_->last_entry()) {
            last_log_term = LogStore_->last_entry()->get_term();
        }

        ulong state_term = state_->get_term();

        output_info( "[ELECTION TIMEOUT] current role: %s, log last term %" PRIu64 ", "
              "state term %" PRIu64 ", target p %d, my p %d, %s, %s",
              srv_role_to_string(role_).c_str(), last_log_term, state_term,
              target_priority_, my_priority_,
              (hb_alive_) ? "hb alive" : "hb dead",
              (pre_vote_.done_) ? "pre-vote done" : "pre-vote NOT done");

        // `term` changed, cannot use previous pre-vote result.
        if (pre_vote_.term_ != state_term) {
            output_info("pre-vote term (%" PRIu64 ") is different, reset it to %" PRIu64 "",
                 pre_vote_.term_, state_term);
            pre_vote_.reset(state_term);
        }

        if ( !peers_.size() ||
             pre_vote_.done_ ||
             get_quorum_for_election() == 0 ) {
            initiate_vote();
        } else {
            request_prevote();
        }

    }

    // restart the election timer if this is not yet a leader
    if (role_ != srv_role::leader) {
        restart_election_timer();
    }
}

void raft_server::cancel_schedulers() {
    if (!scheduler_) {
        // Already cancelled.
        return;
    }

    if (election_task_) {
        cancel_task(election_task_);
    }

    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        const ptr<peer>& p = it->second;
        if (p->get_hb_task()) {
            cancel_task(p->get_hb_task());
        }
        // Shutdown peer to cut off smart pointers.
        p->shutdown();

        // Free user context of Snapshot if exists.
        clear_Snapshot_sync_ctx(*p);
    }
    scheduler_.reset();
}

void raft_server::schedule_task(ptr<delayed_task>& task, int32 milliseconds) {
    if (stopping_) return;

    if (!scheduler_) {
        std::lock_guard<std::mutex> l(ctx_->ctx_lock_);
        scheduler_ = ctx_->scheduler_;
    }
    if (scheduler_) {
        scheduler_->schedule(task, milliseconds);
    }
}

void raft_server::cancel_task(ptr<delayed_task>& task) {
    if (!scheduler_) return;
    scheduler_->cancel(task);
}

}// namespace SDN_Raft;

