#ifndef _LOG_VALUE_TYPE_HXX_
#define _LOG_VALUE_TYPE_HXX_

namespace SDN_Raft {

/*日志类型枚举*/
enum LogType : uint8_t {
    app_log         = 1,
    conf            = 2,
    cluster_server  = 3,
    log_pack        = 4,
    snp_sync_req    = 5,
    custom          = 231,
};

}
#endif 
