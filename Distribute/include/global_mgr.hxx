
#ifndef _GLOBAL_MGR_HXX_
#define _GLOBAL_MGR_HXX_

#include "asio_service_options.hxx"
#include "util.hxx"
#include "ptr.hxx"

#include <atomic>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace SDN_Raft {

class asio_service;
class Logger;
class raft_server;

class global_mgr {
    __interface_body__(global_mgr);

public :
    /**
     * This function is called by the constructor of `raft_server`.
     *
     * @param server Raft server instance.
     */
    virtual void init_raft_server(raft_server* server) = 0;

    /**
     * This function is called by the destructor of `raft_server`.
     *
     * @param server Raft server instance.
     */
    virtual void close_raft_server(raft_server* server) = 0;

    /**
     * Request `append_entries` for the given server.
     *
     * @param server Raft server instance to request `append_entries`.
     */
    virtual void request_append(ptr<raft_server> server) = 0;

    /**
     * Request background commit execution for the given server.
     *
     * @param server Raft server instance to execute commit.
     */
    virtual void request_commit(ptr<raft_server> server) = 0;

};


/**
 * Configurations for the initialization of `SDN_Raft_global_mgr`.
 */
struct SDN_Raft_global_config {
    SDN_Raft_global_config()
        : num_commit_threads_(1)
        , num_append_threads_(1)
        , max_scheduling_unit_ms_(200)
        {}

    /**
     * The number of globally shared threads executing the
     * commit of state machine.
     */
    size_t num_commit_threads_;

    /**
     * The number of globally shared threads executing replication.
     */
    size_t num_append_threads_;

    /**
     * If a commit of a Raft instance takes longer than this time,
     * worker thread will pause the commit of the current instance
     * and schedule the next instance, to avoid starvation issue.
     */
    size_t max_scheduling_unit_ms_;
};

static SDN_Raft_global_config __DEFAULT_NURAFT_GLOBAL_CONFIG;

class SDN_Raft_global_mgr : public global_mgr {
public:
    SDN_Raft_global_mgr();

    virtual ~SDN_Raft_global_mgr();

    __nocopy__(SDN_Raft_global_mgr);
public:

    /**
     * Initialize the global instance.
     *
     * @return If succeeds, the initialized instance.
     *         If already initialized, the existing instance.
     */
    static SDN_Raft_global_mgr* init(const SDN_Raft_global_config& config =
                                       __DEFAULT_NURAFT_GLOBAL_CONFIG);

    /**
     * Shutdown the global instance and free all resources.
     * All Raft instances should be shut down before calling this API.
     */
    static void shutdown();

    /**
     * Get the current global instance.
     *
     * @return The current global instance if initialized.
     *         `nullptr` if not initialized.
     */
    static SDN_Raft_global_mgr* get_instance();

    /**
     * Initialize a global Asio service.
     * Return the existing one if already initialized.
     *
     * @param asio_opt Asio service options.
     * @param Logger_inst Logger instance.
     * @return Asio service instance.
     */
    static ptr<asio_service> init_asio_service(
        const asio_service_options& asio_opt = asio_service_options(),
        ptr<Logger> Logger_inst = nullptr);

    /**
     * Get the global Asio service instance.
     *
     * @return Asio service instance.
     *         `nullptr` if not initialized.
     */
    static ptr<asio_service> get_asio_service();

    /**
     * This function is called by the constructor of `raft_server`.
     *
     * @param server Raft server instance.
     */
    virtual void init_raft_server(raft_server* server) __override__;

    /**
     * This function is called by the destructor of `raft_server`.
     *
     * @param server Raft server instance.
     */
    virtual void close_raft_server(raft_server* server) __override__;

    /**
     * Request `append_entries` for the given server.
     *
     * @param server Raft server instance to request `append_entries`.
     */
    virtual void request_append(ptr<raft_server> server) __override__;

    /**
     * Request background commit execution for the given server.
     *
     * @param server Raft server instance to execute commit.
     */
    virtual void request_commit(ptr<raft_server> server) __override__;

private:
    struct worker_handle;

    /**
     * Initialize thread pool with the given config.
     */
    void init_thread_pool();

    /**
     * Loop for commit worker threads.
     */
    void commit_worker_loop(ptr<worker_handle> handle);

    /**
     * Loop for append worker threads.
     */
    void append_worker_loop(ptr<worker_handle> handle);

    /**
     * Lock for global Asio service instance.
     */
    std::mutex asio_service_lock_;

    /**
     * Global Asio service instance.
     */
    ptr<asio_service> asio_service_;

    /**
     * Global config.
     */
    SDN_Raft_global_config config_;

    /**
     * Counter for assigning thread ID.
     */
    std::atomic<size_t> thread_id_counter_;

    /**
     * Commit thread pool.
     */
    std::vector< ptr<worker_handle> > commit_workers_;

    /**
     * Commit thread pool.
     */
    std::vector< ptr<worker_handle> > append_workers_;

    /**
     * Commit requests.
     * Duplicate requests from the same `raft_server` will not be allowed.
     */
    std::list< ptr<raft_server> > commit_queue_;

    /**
     * A set for efficient duplicate checking of `raft_server`.
     * It will contain all `raft_server`s currently in `commit_queue_`.
     */
    std::unordered_set< ptr<raft_server> > commit_server_set_;

    /**
     * Lock for `commit_queue_` and `commit_server_set_`.
     */
    std::mutex commit_queue_lock_;

    /**
     * Append (replication) requests.
     * Duplicate requests from the same `raft_server` will not be allowed.
     */
    std::list< ptr<raft_server> > append_queue_;

    /**
     * A set for efficient duplicate checking of `raft_server`.
     * It will contain all `raft_server`s currently in `append_queue_`.
     */
    std::unordered_set< ptr<raft_server> > append_server_set_;

    /**
     * Lock for `append_queue_` and `append_server_set_`.
     */
    std::mutex append_queue_lock_;
};

} // namespace SDN_Raft;

#endif
