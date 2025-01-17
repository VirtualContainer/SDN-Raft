#include "crc32.hxx"
#include "LogEntry.hxx"

namespace SDN_Raft {
LogEntry::LogEntry(ulong term,
                     const ptr<Buffer>& buff,
                     LogType value_type,
                     uint64_t log_timestamp,
                     bool has_crc32,
                     uint32_t crc32,
                     bool compute_crc)
    : term_(term)
    , value_type_(value_type)
    , buff_(buff)
    , timestamp_us_(log_timestamp)
    , has_crc32_(has_crc32)
    , crc32_(crc32)
    {
        if (buff_ && !has_crc32 && compute_crc) {
            has_crc32_ = true;
            crc32_ = crc32_8(buff_->data_begin(),
                             buff_->size(),
                             0);
        }
    }

void LogEntry::change_buf(const ptr<Buffer>& buff) {
    buff_ = buff;
    if (buff_ && has_crc32_) {
        crc32_ = crc32_8(buff_->data_begin(),
                         buff_->size(),
                         0);
    }
}
}