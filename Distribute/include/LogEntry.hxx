#ifndef _LOG_ENTRY_HXX_
#define _LOG_ENTRY_HXX_

#include "util.hxx"
#include "Buffer.hxx"
#include "LogType.hxx"
#include "ptr.hxx"
#include <cstdint>

#ifdef _NO_EXCEPTION
#include <cassert>
#endif
#include <stdexcept>

namespace SDN_Raft {

class LogEntry {
public:
    LogEntry(ulong term,
              const ptr<Buffer>& buff,
              LogType value_type = LogType::app_log,
              uint64_t log_timestamp = 0,
              bool has_crc32 = false,
              uint32_t crc32 = 0,
              bool compute_crc = true);

    __nocopy__(LogEntry);

public:
    ulong get_term() const {
        return term_;
    }

    void set_term(ulong term) {
        term_ = term;
    }

    LogType get_val_type() const {
        return value_type_;
    }

    bool is_buf_null() const {
        return (buff_.get()) ? false : true;
    }

    Buffer& get_buf() const {
        // We accept nil Buffer, but in that case,
        // the get_buf() shouldn't be called, throw runtime exception
        // instead of having segment fault (AV on Windows)
        if (!buff_) {
#ifndef _NO_EXCEPTION
            throw std::runtime_error("get_buf cannot be called for a LogEntry "
                                     "with nil Buffer");
#else
            assert(0);
#endif
        }

        return *buff_;
    }

    ptr<Buffer> get_buf_ptr() const {
        return buff_;
    }

    void change_buf(const ptr<Buffer>& buff);

    uint64_t get_timestamp() const {
        return timestamp_us_;
    }

    void set_timestamp(uint64_t t) {
        timestamp_us_ = t;
    }

    bool has_crc32() const {
        return has_crc32_;
    }

    uint32_t get_crc32() const {
        return crc32_;
    }

    void set_crc32(uint32_t crc) {
        crc32_ = crc;
    }

    ptr<Buffer> serialize() {
        buff_->pos(0);
        ptr<Buffer> buf = Buffer::alloc( sizeof(ulong) +
                                         sizeof(char) +
                                         buff_->size() );
        buf->put(term_);
        buf->put( (static_cast<byte>(value_type_)) );
        buf->put(*buff_);
        buf->pos(0);
        return buf;
    }

    static ptr<LogEntry> deserialize(Buffer& buf) {
        ulong term = buf.get_ulong();
        LogType t = static_cast<LogType>(buf.get_byte());
        ptr<Buffer> data = Buffer::copy(buf);
        return cs_new<LogEntry>(term, data, t);
    }

    static ulong term_in_Buffer(Buffer& buf) {
        ulong term = buf.get_ulong();
        buf.pos(0); // reset the position
        return term;
    }

private:
    /**
     * The term number when this log entry was generated.
     */
    ulong term_;

    /**
     * Type of this log entry.
     */
    LogType value_type_;

    /**
     * Actual data that this log entry carries.
     */
    ptr<Buffer> buff_;

    /**
     * The timestamp (since epoch) when this log entry was generated
     * in microseconds. Used only when `replicate_log_timestamp_` in
     * `asio_service_options` is set.
     */
    uint64_t timestamp_us_;

    /**
     * CRC32 checksum of this log entry.
     * Used only when `crc_on_payload` in `asio_service_options` is set.
     */
    bool has_crc32_;
    uint32_t crc32_;
};

}

#endif //_LOG_ENTRY_HXX_

