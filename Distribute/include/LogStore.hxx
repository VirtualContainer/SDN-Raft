#ifndef _LOG_STORE_HXX_
#define _LOG_STORE_HXX_

#include "async.hxx"
#include "Buffer.hxx"
#include "LogEntry.hxx"
#include "util.hxx"
#include "ptr.hxx"

#include <vector>

namespace SDN_Raft {

class LogStore {
    __interface_body__(LogStore);

public:
  
    virtual ulong next_slot() const = 0;
    virtual ulong start_index() const = 0;
    virtual ptr<LogEntry> last_entry() const = 0;
    virtual ulong append(ptr<LogEntry>& entry) = 0;
    virtual void write_at(ulong index, ptr<LogEntry>& entry) = 0;
    virtual void end_of_append_batch(ulong start, ulong cnt) {}
    virtual ptr<std::vector<ptr<LogEntry>>> log_entries(ulong start, ulong end) = 0;
    virtual ptr<std::vector<ptr<LogEntry>>> log_entries_ext(
            ulong start, ulong end, int64 batch_size_hint_in_bytes = 0) {
        return log_entries(start, end);
    }
    virtual ptr<LogEntry> entry_at(ulong index) = 0;
    virtual ulong term_at(ulong index) = 0;

    virtual ptr<Buffer> pack(ulong index, int32 cnt) = 0;
    virtual void apply_pack(ulong index, Buffer& pack) = 0;
    virtual bool compact(ulong last_log_index) = 0;   
    virtual void compact_async(ulong last_log_index,
                               const async_result<bool>::handler_type& when_done) {
        bool rc = compact(last_log_index);
        ptr<std::exception> exp(nullptr);
        when_done(rc, exp);
    }
   
    virtual bool flush() = 0;

    virtual ulong last_durable_index() { return next_slot() - 1; }
};

class in_memory_LogStore;
class in_disk_LogStore;

}

#endif
