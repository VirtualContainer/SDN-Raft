#ifndef _STATE_MGR_HXX_
#define _STATE_MGR_HXX_

#include "util.hxx"
#include "ptr.hxx"

namespace SDN_Raft {

class ClusterConfig;
class LogStore;
class srv_state;
class state_mgr {
    __interface_body__(state_mgr);

public:
    /**
     * Load the last saved cluster config.
     * This function will be invoked on initialization of
     * Raft server.
     *
     * Even at the very first initialization, it should
     * return proper initial cluster config, not `nullptr`.
     * The initial cluster config must include the server itself.
     *
     * @return Cluster config.
     */
    virtual ptr<ClusterConfig> load_config() = 0;

    /**
     * Save given cluster config.
     *
     * @param config Cluster config to save.
     */
    virtual void save_config(const ClusterConfig& config) = 0;

    /**
     * Save given server state.
     *
     * @param state Server state to save.
     */
    virtual void save_state(const srv_state& state) = 0;

    /**
     * Load the last saved server state.
     * This function will be invoked on initialization of
     * Raft server
     *
     * At the very first initialization, it should return
     * `nullptr`.
     *
     * @param Server state.
     */
    virtual ptr<srv_state> read_state() = 0;

    /**
     * Get instance of user-defined Raft log store.
     *
     * @param Raft log store instance.
     */
    virtual ptr<LogStore> load_LogStore() = 0;

    /**
     * Get ID of this Raft server.
     *
     * @return Server ID.
     */
    virtual int32 server_id() = 0;

    /**
     * System exit handler. This function will be invoked on
     * abnormal termination of Raft server.
     *
     * @param exit_code Error code.
     */
    virtual void system_exit(const int exit_code) = 0;
};

}

#endif //_STATE_MGR_HXX_
