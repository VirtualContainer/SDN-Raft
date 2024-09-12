#include "Buffer.hxx"

#include <cstring>
#include <stdexcept>

#define put16l(val, ptr) {          \
    ptr[0] = (val >>  0) & 0xff;    \
    ptr[1] = (val >>  8) & 0xff;    }

#define put16b(val, ptr) {          \
    ptr[1] = (val >>  0) & 0xff;    \
    ptr[0] = (val >>  8) & 0xff;    }

#define put32l(val, ptr) {          \
    ptr[0] = (val >>  0) & 0xff;    \
    ptr[1] = (val >>  8) & 0xff;    \
    ptr[2] = (val >> 16) & 0xff;    \
    ptr[3] = (val >> 24) & 0xff;    }

#define put32b(val, ptr) {          \
    ptr[3] = (val >>  0) & 0xff;    \
    ptr[2] = (val >>  8) & 0xff;    \
    ptr[1] = (val >> 16) & 0xff;    \
    ptr[0] = (val >> 24) & 0xff;    }

#define put64l(val, ptr) {          \
    ptr[0] = (val >>  0) & 0xff;    \
    ptr[1] = (val >>  8) & 0xff;    \
    ptr[2] = (val >> 16) & 0xff;    \
    ptr[3] = (val >> 24) & 0xff;    \
    ptr[4] = (val >> 32) & 0xff;    \
    ptr[5] = (val >> 40) & 0xff;    \
    ptr[6] = (val >> 48) & 0xff;    \
    ptr[7] = (val >> 56) & 0xff;    }

#define put64b(val, ptr) {          \
    ptr[7] = (val >>  0) & 0xff;    \
    ptr[6] = (val >>  8) & 0xff;    \
    ptr[5] = (val >> 16) & 0xff;    \
    ptr[4] = (val >> 24) & 0xff;    \
    ptr[3] = (val >> 32) & 0xff;    \
    ptr[2] = (val >> 40) & 0xff;    \
    ptr[1] = (val >> 48) & 0xff;    \
    ptr[0] = (val >> 56) & 0xff;    }


#define get16l(ptr, val) {              \
    uint16_t tmp = ptr[1]; tmp <<= 8;   \
    tmp |= ptr[0];                      \
    val = tmp;                          }

#define get16b(ptr, val) {              \
    uint16_t tmp = ptr[0]; tmp <<= 8;   \
    tmp |= ptr[1];                      \
    val = tmp;                          }

#define get32l(ptr, val) {              \
    uint32_t tmp = ptr[3]; tmp <<= 8;   \
    tmp |= ptr[2]; tmp <<= 8;           \
    tmp |= ptr[1]; tmp <<= 8;           \
    tmp |= ptr[0];                      \
    val = tmp;                          }

#define get32b(ptr, val) {              \
    uint32_t tmp = ptr[0]; tmp <<= 8;   \
    tmp |= ptr[1]; tmp <<= 8;           \
    tmp |= ptr[2]; tmp <<= 8;           \
    tmp |= ptr[3];                      \
    val = tmp;                          }

#define get64l(ptr, val) {              \
    uint64_t tmp = ptr[7]; tmp <<= 8;   \
    tmp |= ptr[6]; tmp <<= 8;           \
    tmp |= ptr[5]; tmp <<= 8;           \
    tmp |= ptr[4]; tmp <<= 8;           \
    tmp |= ptr[3]; tmp <<= 8;           \
    tmp |= ptr[2]; tmp <<= 8;           \
    tmp |= ptr[1]; tmp <<= 8;           \
    tmp |= ptr[0];                      \
    val = tmp;                          }

#define get64b(ptr, val) {              \
    uint64_t tmp = ptr[0]; tmp <<= 8;   \
    tmp |= ptr[1]; tmp <<= 8;           \
    tmp |= ptr[2]; tmp <<= 8;           \
    tmp |= ptr[3]; tmp <<= 8;           \
    tmp |= ptr[4]; tmp <<= 8;           \
    tmp |= ptr[5]; tmp <<= 8;           \
    tmp |= ptr[6]; tmp <<= 8;           \
    tmp |= ptr[7];                      \
    val = tmp;                          }


#define chk_length(val) \
    if ( !is_valid( sizeof(val) ) ) throw std::overflow_error("not enough space")

namespace SDN_Raft {

BufferSerializer::BufferSerializer(Buffer& src_buf,
                                     BufferSerializer::endianness endian)
    : endian_(endian)
    , buf_(src_buf)
    , pos_(0)
{}

BufferSerializer::BufferSerializer(ptr<Buffer>& src_buf_ptr,
                                     BufferSerializer::endianness endian)
    : endian_(endian)
    , buf_(*src_buf_ptr)
    , pos_(0)
{}

size_t BufferSerializer::size() const {
    return buf_.size();
}

void BufferSerializer::pos(size_t new_pos) {
    if (new_pos > buf_.size()) throw std::overflow_error("invalid position");
    pos_ = new_pos;
}

void* BufferSerializer::data() const {
    uint8_t* ptr = (uint8_t*)buf_.data_begin();
    return ptr + pos();
}

bool BufferSerializer::is_valid(size_t len) const {
    if ( pos() + len > buf_.size() ) return false;
    return true;
}

void BufferSerializer::put_u8(uint8_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    ptr[0] = val;
    pos( pos() + sizeof(val) );
}

void BufferSerializer::put_u16(uint16_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put16l(val, ptr); }
    else                    { put16b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferSerializer::put_u32(uint32_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put32l(val, ptr); }
    else                    { put32b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferSerializer::put_u64(uint64_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put64l(val, ptr); }
    else                    { put64b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferSerializer::put_i8(int8_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    ptr[0] = val;
    pos( pos() + sizeof(val) );
}

void BufferSerializer::put_i16(int16_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put16l(val, ptr); }
    else                    { put16b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferSerializer::put_i32(int32_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put32l(val, ptr); }
    else                    { put32b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferSerializer::put_i64(int64_t val) {
    chk_length(val);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { put64l(val, ptr); }
    else                    { put64b(val, ptr); }
    pos( pos() + sizeof(val) );
}

void BufferSerializer::put_raw(const void* raw_ptr, size_t len) {
    if ( !is_valid(len) ) throw std::overflow_error("not enough space");
    memcpy(data(), raw_ptr, len);
    pos( pos() + len );
}

void BufferSerializer::put_buffer(const Buffer& buf) {
    size_t len = buf.size() - buf.pos();
    put_raw(buf.data(), len);
}

void BufferSerializer::put_bytes(const void* raw_ptr, size_t len) {
    if ( !is_valid(len + sizeof(uint32_t)) ) {
        throw std::overflow_error("not enough space");
    }
    put_u32(len);
    put_raw(raw_ptr, len);
}

void BufferSerializer::put_str(const std::string& str) {
    put_bytes( str.data(), str.size() );
}

void BufferSerializer::put_cstr(const char* str) {
    size_t local_pos = pos_;
    size_t buf_size = buf_.size();
    char* ptr = (char*)buf_.data_begin();

    size_t ii = 0;
    while (str[ii] != 0x0) {
        if (local_pos >= buf_size) {
            throw std::overflow_error("not enough space");
        }
        ptr[local_pos] = str[ii];
        local_pos++;
        ii++;
    }
    // Put NULL character at the end.
    if (local_pos >= buf_size) {
        throw std::overflow_error("not enough space");
    }
    ptr[local_pos++] = 0x0;
    pos( local_pos );
}

uint8_t BufferSerializer::get_u8() {
    uint8_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    ret = ptr[0];
    pos( pos() + sizeof(ret) );
    return ret;
}

uint16_t BufferSerializer::get_u16() {
    uint16_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get16l(ptr, ret); }
    else                    { get16b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

uint32_t BufferSerializer::get_u32() {
    uint32_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get32l(ptr, ret); }
    else                    { get32b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

uint64_t BufferSerializer::get_u64() {
    uint64_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get64l(ptr, ret); }
    else                    { get64b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

int8_t BufferSerializer::get_i8() {
    int8_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    ret = ptr[0];
    pos( pos() + sizeof(ret) );
    return ret;
}

int16_t BufferSerializer::get_i16() {
    int16_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get16l(ptr, ret); }
    else                    { get16b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

int32_t BufferSerializer::get_i32() {
    int32_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get32l(ptr, ret); }
    else                    { get32b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

int64_t BufferSerializer::get_i64() {
    int64_t ret = 0;
    chk_length(ret);
    uint8_t* ptr = buf_.data_begin() + pos_;
    if (endian_ == LITTLE)  { get64l(ptr, ret); }
    else                    { get64b(ptr, ret); }
    pos( pos() + sizeof(ret) );
    return ret;
}

void* BufferSerializer::get_raw(size_t len) {
    uint8_t* ptr = buf_.data_begin() + pos_;
    pos( pos() + len );
    return ptr;
}

void BufferSerializer::get_buffer(ptr<Buffer>& dst) {
    size_t len = dst->size() - dst->pos();
    void* ptr = get_raw(len);
    ::memcpy(dst->data(), ptr, len);
}

void* BufferSerializer::get_bytes(size_t& len) {
    len = get_u32();
    if ( !is_valid(len) ) throw std::overflow_error("not enough space");
    return get_raw(len);
}

std::string BufferSerializer::get_str() {
    size_t len = 0;
    void* data = get_bytes(len);
    if (!data) return std::string();
    return std::string((const char*)data, len);
}

const char* BufferSerializer::get_cstr() {
    char* ptr = (char*)buf_.data_begin() + pos_;
    size_t len = strlen(ptr);
    pos( pos() + len + 1 );
    return ptr;
}

}

