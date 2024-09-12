#include "LogStore.hxx"
#include "raft_server.hxx"
#include "Awaiter.hxx"
#include "Timer.hxx"

#include <map>
#include <thread>
#include <atomic>
#include <cassert>

namespace SDN_Raft {


class in_memory_LogStore : public LogStore {
public:
    in_memory_LogStore();

    ~in_memory_LogStore();

    __nocopy__(in_memory_LogStore);

public:
    ulong next_slot() const;

    ulong start_index() const;

    ptr<LogEntry> last_entry() const;

    ulong append(ptr<LogEntry>& entry);

    void write_at(ulong index, ptr<LogEntry>& entry);

    ptr<std::vector<ptr<LogEntry>>> log_entries(ulong start, ulong end);

    ptr<std::vector<ptr<LogEntry>>> log_entries_ext(
            ulong start, ulong end, int64 batch_size_hint_in_bytes = 0);

    ptr<LogEntry> entry_at(ulong index);

    ulong term_at(ulong index);

    ptr<Buffer> pack(ulong index, int32 cnt);

    void apply_pack(ulong index, Buffer& pack);

    bool compact(ulong last_log_index);

    bool flush();

    void close();

    ulong last_durable_index();

    void set_disk_delay(raft_server* raft, size_t delay_ms);

private:
    static ptr<LogEntry> make_clone(const ptr<LogEntry>& entry);

    void disk_emul_loop();

    /**
     * Map of <log index, log data>.
     */
    std::map<ulong, ptr<LogEntry>> logs_;

    /**
     * Lock for `logs_`.
     */
    mutable std::mutex logs_lock_;

    /**
     * The index of the first log.
     */
    std::atomic<ulong> start_idx_;

    /**
     * Backward pointer to Raft server.
     */
    raft_server* raft_server_bwd_pointer_;

    // Testing purpose --------------- BEGIN

    /**
     * If non-zero, this log store will emulate the disk write delay.
     */
    std::atomic<size_t> disk_emul_delay;

    /**
     * Map of <timestamp, log index>, emulating logs that is being written to disk.
     * Log index will be regarded as "durable" after the corresponding timestamp.
     */
    std::map<uint64_t, uint64_t> disk_emul_logs_being_written_;

    /**
     * Thread that will update `last_durable_index_` and call
     * `notify_log_append_completion` at proper time.
     */
    std::unique_ptr<std::thread> disk_emul_thread_;

    /**
     * Flag to terminate the thread.
     */
    std::atomic<bool> disk_emul_thread_stop_signal_;

    /**
     * Event awaiter that emulates disk delay.
     */
    EventAwaiter disk_emul_ea_;

    /**
     * Last written log index.
     */
    std::atomic<uint64_t> disk_emul_last_durable_index_;

   
};


in_memory_LogStore::in_memory_LogStore()
    : start_idx_(1)
    , raft_server_bwd_pointer_(nullptr)
    , disk_emul_delay(0)
    , disk_emul_thread_(nullptr)
    , disk_emul_thread_stop_signal_(false)
    , disk_emul_last_durable_index_(0)
{
    // Dummy entry for index 0.
    ptr<Buffer> buf = Buffer::alloc(sz_ulong);
    logs_[0] = cs_new<LogEntry>(0, buf);
}

in_memory_LogStore::~in_memory_LogStore() {
    if (disk_emul_thread_) {
        disk_emul_thread_stop_signal_ = true;
        disk_emul_ea_.invoke();
        if (disk_emul_thread_->joinable()) {
            disk_emul_thread_->join();
        }
    }
}

ptr<LogEntry> in_memory_LogStore::make_clone(const ptr<LogEntry>& entry) {
    // NOTE:
    //   Timestamp is used only when `replicate_log_timestamp_` option is on.
    //   Otherwise, log store does not need to store or load it.
    ptr<LogEntry> clone = cs_new<LogEntry>
                           ( entry->get_term(),
                             Buffer::clone( entry->get_buf() ),
                             entry->get_val_type(),
                             entry->get_timestamp(),
                             entry->has_crc32(),
                             entry->get_crc32(),
                             false );
    return clone;
}

ulong in_memory_LogStore::next_slot() const {
    std::lock_guard<std::mutex> l(logs_lock_);
    // Exclude the dummy entry.
    return start_idx_ + logs_.size() - 1;
}

ulong in_memory_LogStore::start_index() const {
    return start_idx_;
}

ptr<LogEntry> in_memory_LogStore::last_entry() const {
    ulong next_idx = next_slot();
    std::lock_guard<std::mutex> l(logs_lock_);
    auto entry = logs_.find( next_idx - 1 );
    if (entry == logs_.end()) {
        entry = logs_.find(0);
    }

    return make_clone(entry->second);
}

ulong in_memory_LogStore::append(ptr<LogEntry>& entry) {
    ptr<LogEntry> clone = make_clone(entry);

    std::lock_guard<std::mutex> l(logs_lock_);
    size_t idx = start_idx_ + logs_.size() - 1;
    logs_[idx] = clone;

    if (disk_emul_delay) {
        uint64_t cur_time = Timer::get_timeofday_us();
        disk_emul_logs_being_written_[cur_time + disk_emul_delay * 1000] = idx;
        disk_emul_ea_.invoke();
    }

    return idx;
}

void in_memory_LogStore::write_at(ulong index, ptr<LogEntry>& entry) {
    ptr<LogEntry> clone = make_clone(entry);

    // Discard all logs equal to or greater than `index.
    std::lock_guard<std::mutex> l(logs_lock_);
    auto itr = logs_.lower_bound(index);
    while (itr != logs_.end()) {
        itr = logs_.erase(itr);
    }
    logs_[index] = clone;

    if (disk_emul_delay) {
        uint64_t cur_time = Timer::get_timeofday_us();
        disk_emul_logs_being_written_[cur_time + disk_emul_delay * 1000] = index;

        // Remove entries greater than `index`.
        auto entry = disk_emul_logs_being_written_.begin();
        while (entry != disk_emul_logs_being_written_.end()) {
            if (entry->second > index) {
                entry = disk_emul_logs_being_written_.erase(entry);
            } else {
                entry++;
            }
        }
        disk_emul_ea_.invoke();
    }
}

ptr< std::vector< ptr<LogEntry> > >
    in_memory_LogStore::log_entries(ulong start, ulong end)
{
    ptr< std::vector< ptr<LogEntry> > > ret =
        cs_new< std::vector< ptr<LogEntry> > >();

    ret->resize(end - start);
    ulong cc=0;
    for (ulong ii = start ; ii < end ; ++ii) {
        ptr<LogEntry> src = nullptr;
        {   std::lock_guard<std::mutex> l(logs_lock_);
            auto entry = logs_.find(ii);
            if (entry == logs_.end()) {
                entry = logs_.find(0);
                assert(0);
            }
            src = entry->second;
        }
        (*ret)[cc++] = make_clone(src);
    }
    return ret;
}

ptr<std::vector<ptr<LogEntry>>>
    in_memory_LogStore::log_entries_ext(ulong start,
                                     ulong end,
                                     int64 batch_size_hint_in_bytes)
{
    ptr< std::vector< ptr<LogEntry> > > ret =
        cs_new< std::vector< ptr<LogEntry> > >();

    if (batch_size_hint_in_bytes < 0) {
        return ret;
    }

    size_t accum_size = 0;
    for (ulong ii = start ; ii < end ; ++ii) {
        ptr<LogEntry> src = nullptr;
        {   std::lock_guard<std::mutex> l(logs_lock_);
            auto entry = logs_.find(ii);
            if (entry == logs_.end()) {
                entry = logs_.find(0);
                assert(0);
            }
            src = entry->second;
        }
        ret->push_back(make_clone(src));
        accum_size += src->get_buf().size();
        if (batch_size_hint_in_bytes &&
            accum_size >= (ulong)batch_size_hint_in_bytes) break;
    }
    return ret;
}

ptr<LogEntry> in_memory_LogStore::entry_at(ulong index) {
    ptr<LogEntry> src = nullptr;
    {   std::lock_guard<std::mutex> l(logs_lock_);
        auto entry = logs_.find(index);
        if (entry == logs_.end()) {
            entry = logs_.find(0);
        }
        src = entry->second;
    }
    return make_clone(src);
}

ulong in_memory_LogStore::term_at(ulong index) {
    ulong term = 0;
    {   std::lock_guard<std::mutex> l(logs_lock_);
        auto entry = logs_.find(index);
        if (entry == logs_.end()) {
            entry = logs_.find(0);
        }
        term = entry->second->get_term();
    }
    return term;
}

ptr<Buffer> in_memory_LogStore::pack(ulong index, int32 cnt) {
    std::vector< ptr<Buffer> > logs;

    size_t size_total = 0;
    for (ulong ii=index; ii<index+cnt; ++ii) {
        ptr<LogEntry> le = nullptr;
        {   std::lock_guard<std::mutex> l(logs_lock_);
            le = logs_[ii];
        }
        assert(le.get());
        ptr<Buffer> buf = le->serialize();
        size_total += buf->size();
        logs.push_back( buf );
    }

    ptr<Buffer> buf_out = Buffer::alloc
                          ( sizeof(int32) +
                            cnt * sizeof(int32) +
                            size_total );
    buf_out->pos(0);
    buf_out->put((int32)cnt);

    for (auto& entry: logs) {
        ptr<Buffer>& bb = entry;
        buf_out->put((int32)bb->size());
        buf_out->put(*bb);
    }
    return buf_out;
}

void in_memory_LogStore::apply_pack(ulong index, Buffer& pack) {
    pack.pos(0);
    int32 num_logs = pack.get_int();

    for (int32 ii=0; ii<num_logs; ++ii) {
        ulong cur_idx = index + ii;
        int32 buf_size = pack.get_int();

        ptr<Buffer> buf_local = Buffer::alloc(buf_size);
        pack.get(buf_local);

        ptr<LogEntry> le = LogEntry::deserialize(*buf_local);
        {   std::lock_guard<std::mutex> l(logs_lock_);
            logs_[cur_idx] = le;
        }
    }

    {   std::lock_guard<std::mutex> l(logs_lock_);
        auto entry = logs_.upper_bound(0);
        if (entry != logs_.end()) {
            start_idx_ = entry->first;
        } else {
            start_idx_ = 1;
        }
    }
}

bool in_memory_LogStore::compact(ulong last_log_index) {
    std::lock_guard<std::mutex> l(logs_lock_);
    for (ulong ii = start_idx_; ii <= last_log_index; ++ii) {
        auto entry = logs_.find(ii);
        if (entry != logs_.end()) {
            logs_.erase(entry);
        }
    }

    // WARNING:
    //   Even though nothing has been erased,
    //   we should set `start_idx_` to new index.
    if (start_idx_ <= last_log_index) {
        start_idx_ = last_log_index + 1;
    }
    return true;
}

bool in_memory_LogStore::flush() {
    disk_emul_last_durable_index_ = next_slot() - 1;
    return true;
}

void in_memory_LogStore::close() {}

void in_memory_LogStore::set_disk_delay(raft_server* raft, size_t delay_ms) {
    disk_emul_delay = delay_ms;
    raft_server_bwd_pointer_ = raft;

    if (!disk_emul_thread_) {
        disk_emul_thread_ =
            std::unique_ptr<std::thread>(
                new std::thread(&in_memory_LogStore::disk_emul_loop, this) );
    }
}

ulong in_memory_LogStore::last_durable_index() {
    uint64_t last_log = next_slot() - 1;
    if (!disk_emul_delay) {
        return last_log;
    }

    return disk_emul_last_durable_index_;
}

void in_memory_LogStore::disk_emul_loop() {
    // This thread mimics async disk writes.

    size_t next_sleep_us = 100 * 1000;
    while (!disk_emul_thread_stop_signal_) {
        disk_emul_ea_.wait_us(next_sleep_us);
        disk_emul_ea_.reset();
        if (disk_emul_thread_stop_signal_) break;

        uint64_t cur_time = Timer::get_timeofday_us();
        next_sleep_us = 100 * 1000;

        bool call_notification = false;
        {
            std::lock_guard<std::mutex> l(logs_lock_);
            // Remove all timestamps equal to or smaller than `cur_time`,
            // and pick the greatest one among them.
            auto entry = disk_emul_logs_being_written_.begin();
            while (entry != disk_emul_logs_being_written_.end()) {
                if (entry->first <= cur_time) {
                    disk_emul_last_durable_index_ = entry->second;
                    entry = disk_emul_logs_being_written_.erase(entry);
                    call_notification = true;
                } else {
                    break;
                }
            }

            entry = disk_emul_logs_being_written_.begin();
            if (entry != disk_emul_logs_being_written_.end()) {
                next_sleep_us = entry->first - cur_time;
            }
        }

        if (call_notification) {
            raft_server_bwd_pointer_->notify_log_append_completion(true);
        }
    }
}

}