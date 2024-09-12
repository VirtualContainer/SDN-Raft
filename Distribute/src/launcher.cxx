#include "launcher.hxx"

// LCOV_EXCL_START

namespace SDN_Raft {

raft_launcher::raft_launcher()
    : asio_svc_(nullptr)
    , asio_listener_(nullptr)
    , raft_instance_(nullptr)
    {}

ptr<raft_server> raft_launcher::init(ptr<state_machine> sm,
                                     ptr<state_mgr> smgr,
                                     ptr<Logger> lg,
                                     int port_number,
                                     const asio_service::options& asio_options,
                                     const raft_params& params_given,
                                     const raft_server::init_options& opt)
{
    asio_svc_ = cs_new<asio_service>(asio_options, lg);
    asio_listener_ = asio_svc_->create_rpc_listener(port_number, lg);
    if (!asio_listener_) return nullptr;

    ptr<delayed_task_scheduler> scheduler = asio_svc_;
    ptr<rpc_client_factory> rpc_cli_factory = asio_svc_;

    context* ctx = new context( smgr,
                                sm,
                                asio_listener_,
                                lg,
                                rpc_cli_factory,
                                scheduler,
                                params_given );
    raft_instance_ = cs_new<raft_server>(ctx, opt);
    asio_listener_->listen( raft_instance_ );
    return raft_instance_;
}

bool raft_launcher::shutdown(size_t time_limit_sec) {
    if (!raft_instance_) return false;

    raft_instance_->shutdown();
    raft_instance_.reset();

    if (asio_listener_) {
        asio_listener_->stop();
        asio_listener_->shutdown();
    }
    if (asio_svc_) {
        asio_svc_->stop();
        size_t count = 0;
        while ( asio_svc_->get_active_workers() &&
                count < time_limit_sec * 100 ) {
            // 10ms per tick.
            Timer::sleep_ms(10);
            count++;
        }
    }
    if (asio_svc_->get_active_workers()) return false;
    return true;
}

}

// LCOV_EXCL_STOP

