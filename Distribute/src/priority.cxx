#include "Timer.hxx"
#include "raft_server.hxx"

#include "ClusterConfig.hxx"
#include "Awaiter.hxx"
#include "peer.hxx"
#include "tracer.hxx"

#include <cassert>
#include <list>
#include <sstream>

namespace SDN_Raft {

raft_server::PrioritySetResult
raft_server::set_priority(const int srv_id,
                          const int new_priority,
                          bool broadcast_when_leader_exists) {
    recur_lock(lock_);

    if (id_ != leader_) {
        output_info("Got set_priority request but I'm not a leader: my ID %d, leader %d",
             id_, leader_.load());

        if (!is_leader_alive()) {
            output_warn("No live leader now, broadcast priority change");
            broadcast_priority_change(srv_id, new_priority);
            return PrioritySetResult::BROADCAST;
        } else if (broadcast_when_leader_exists) {
            output_warn("Leader is present but broadcasting priority change as requested");
            broadcast_priority_change(srv_id, new_priority);
            return PrioritySetResult::BROADCAST;
        }
        return PrioritySetResult::IGNORED;
    }

    if (id_ == srv_id && new_priority == 0) {
        // Step down.
        // Even though current leader (myself) can send append_entries()
        // request, it cannot commit the priority change as it will
        // immediately yield its leadership.
        // So in this case, boradcast this change.
        broadcast_priority_change(srv_id, new_priority);
        return PrioritySetResult::BROADCAST;
    }

    // Clone current cluster config.
    ptr<ClusterConfig> cur_config = get_config();

    // NOTE: Need to honor uncommitted config,
    //       refer to comment in `sync_log_to_new_srv()`
    if (uncommitted_config_) {
        output_info("uncommitted config exists at log %" PRIu64 ", prev log %" PRIu64,
             uncommitted_config_->get_log_idx(),
             uncommitted_config_->get_prev_log_idx());
        cur_config = uncommitted_config_;
    }

    ptr<Buffer> enc_conf = cur_config->serialize();
    ptr<ClusterConfig> cloned_config = ClusterConfig::deserialize(*enc_conf);

    std::list<ptr<srv_config>>& s_confs = cloned_config->get_servers();

    for (auto& entry: s_confs) {
        srv_config* s_conf = entry.get();
        if (s_conf->get_id() == srv_id) {
            output_info("Change server %d priority %d -> %d",
                 srv_id, s_conf->get_priority(), new_priority);
            s_conf->set_priority(new_priority);
        }
    }

    // Create a log for new configuration, it should be replicated.
    cloned_config->set_log_idx(LogStore_->next_slot());
    ptr<Buffer> new_conf_buf(cloned_config->serialize());
    ptr<LogEntry> entry( cs_new<LogEntry>
                          ( state_->get_term(),
                            new_conf_buf,
                            LogType::conf,
                            Timer::get_timeofday_us() ) );

    config_changing_ = true;
    uncommitted_config_ = cloned_config;

    store_LogEntry(entry);
    request_append_entries();
    return PrioritySetResult::SET;
}

void raft_server::broadcast_priority_change(const int srv_id,
                                            const int new_priority)
{
    if (srv_id == id_) {
        my_priority_ = new_priority;
    }
    ptr<ClusterConfig> cur_config = get_config();
    for (auto& entry: cur_config->get_servers()) {
        srv_config* s_conf = entry.get();
        if (s_conf->get_id() == srv_id) {
            output_info("Change server %d priority %d -> %d",
                 srv_id, s_conf->get_priority(), new_priority);
            s_conf->set_priority(new_priority);
        }
    }

    // If there is no live leader now,
    // broadcast this request to all peers.
    for (peer_itor it = peers_.begin(); it != peers_.end(); ++it) {
        ptr<RequestMessage> req( cs_new<RequestMessage>
                          ( state_->get_term(),
                            MessageType::priority_change_request,
                            id_,
                            it->second->get_id(),
                            term_for_log(LogStore_->next_slot() - 1),
                            LogStore_->next_slot() - 1,
                            quick_commit_index_.load() ) );

        // ID + priority
        ptr<Buffer> buf = Buffer::alloc(sz_int * 2);
        BufferSerializer ss(buf);
        ss.put_i32(srv_id);
        ss.put_i32(new_priority);
        ptr<LogEntry> le = cs_new<LogEntry>( state_->get_term(),
                                               buf,
                                               LogType::custom );

        std::vector< ptr<LogEntry> >& v = req->log_entries();
        v.push_back(le);

        ptr<peer> pp = it->second;
        if (pp->make_busy()) {
            pp->send_req(pp, req, resp_handler_);
        } else {
            p_er("peer %d is currently busy, cannot send request",
                 pp->get_id());
        }
    }
}

ptr<resp_msg> raft_server::handle_priority_change_req(RequestMessage& req) {
    // NOTE: now this function is protected by lock.
    ptr<resp_msg> resp
        ( cs_new<resp_msg>
          ( req.get_term(),
            MessageType::priority_change_response,
            id_,
            req.get_src() ) );

    std::vector< ptr<LogEntry> >& v = req.log_entries();
    if (!v.size()) {
        output_warn("no log entry");
        return resp;
    }
    if (v[0]->is_buf_null()) {
        output_warn("empty Buffer");
        return resp;
    }

    Buffer& buf = v[0]->get_buf();
    buf.pos(0);
    if (buf.size() < sz_int * 2) {
        output_warn("wrong Buffer size: %zu", buf.size());
        return resp;
    }

    int32 t_id = buf.get_int();
    int32 t_priority = buf.get_int();

    if (t_id == id_) {
        my_priority_ = t_priority;
    }

    ptr<ClusterConfig> c_conf = get_config();
    for (auto& entry: c_conf->get_servers()) {
        srv_config* s_conf = entry.get();
        if ( s_conf->get_id() == t_id ) {
            resp->accept(LogStore_->next_slot());
            output_info("change peer %d priority: %d -> %d",
                 t_id, s_conf->get_priority(), t_priority);
            s_conf->set_priority(t_priority);
            return resp;
        }
    }
    output_warn("cannot find peer %d", t_id);

    return resp;
}

void raft_server::handle_priority_change_resp(resp_msg& resp) {
    output_info("got response from peer %d: %s",
         resp.get_src(),
         resp.get_accepted() ? "success" : "fail");
}

void raft_server::decay_target_priority() {
    // Gap should be bigger than 10.
    int gap = std::max((int)10, target_priority_ / 5);

    // Should be bigger than 0.
    int32 prev_priority = target_priority_;
    target_priority_ = std::max(1, target_priority_ - gap);
    output_info("[PRIORITY] decay, target %d -> %d, mine %d",
         prev_priority, target_priority_, my_priority_);

    // Once `target_priority_` becomes 1,
    // `priority_change_timer_` starts ticking.
    if (prev_priority > 1) priority_change_timer_.reset();
}

void raft_server::update_target_priority() {
    // Get max priority among all peers, including myself.
    int32 max_priority = my_priority_;
    for (auto& entry: peers_) {
        peer* peer_elem = entry.second.get();
        const srv_config& s_conf = peer_elem->get_config();
        int32 cur_priority = s_conf.get_priority();
        max_priority = std::max(max_priority, cur_priority);
    }
    if (max_priority > 0) {
        target_priority_ = max_priority;
    } else {
        target_priority_ = srv_config::INIT_PRIORITY;
    }
    priority_change_timer_.reset();

    hb_alive_ = true;
    pre_vote_.reset(state_->get_term());
    output_trace("(update) new target priority: %d", target_priority_);
}

} // namespace SDN_Raft;

