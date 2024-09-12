
#ifndef _LAUNCHER_HXX_
#define _LAUNCHER_HXX_

#include "raft.hxx"

namespace SDN_Raft {

/**
 * Helper class to skip the details of ASIO settings.
 */
class raft_launcher {
public:
    raft_launcher();

    /**
     * Initialize ASIO service and Raft server.
     *
     * @param sm State machine.
     * @param smgr State manager.
     * @param lg Logger.
     * @param port_number Port number.
     * @param asio_options ASIO options.
     * @param params Raft parameters.
     * @param opt Raft server init options.
     * @param raft_callback Callback function for hooking the operation.
     * @return Raft server instance.
     *         `nullptr` on any errors.
     */
    ptr<raft_server> init(ptr<state_machine> sm,
                          ptr<state_mgr> smgr,
                          ptr<Logger> lg,
                          int port_number,
                          const asio_service::options& asio_options,
                          const raft_params& params,
                          const raft_server::init_options& opt = raft_server::init_options());

    /**
     * Shutdown Raft server and ASIO service.
     * If this function is hanging even after the given timeout,
     * it will do force return.
     *
     * @param time_limit_sec Waiting timeout in seconds.
     * @return `true` on success.
     */
    bool shutdown(size_t time_limit_sec = 5);

    /**
     * Get ASIO service instance.
     *
     * @return ASIO service instance.
     */
    ptr<asio_service> get_asio_service() const { return asio_svc_; }

    /**
     * Get ASIO listener.
     *
     * @return ASIO listener.
     */
    ptr<rpc_listener> get_rpc_listener() const { return asio_listener_; }

    /**
     * Get Raft server instance.
     *
     * @return Raft server instance.
     */
    ptr<raft_server> get_raft_server() const { return raft_instance_; }

private:
    ptr<asio_service> asio_svc_;
    ptr<rpc_listener> asio_listener_;
    ptr<raft_server> raft_instance_;
};

}

#endif