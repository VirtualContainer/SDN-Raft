#include "Buffer.hxx"
#include "statistics.hxx"

#include <cstring>
#include <iostream>

#define __is_big_block(p)       ( 0x80000000 & *( (uint*)(p) ) )

#define __init_block(ptr, len, type)                                \
                                ( (type*)(ptr) )[0] = (type)len;    \
                                ( (type*)(ptr) )[1] = 0

#define __init_s_block(p, l)    __init_block(p, l, ushort)

#define __init_b_block(p, l)    __init_block(p, l, uint);   \
                                *( (uint*)(p) ) |= 0x80000000

#define __pos_of_s_block(p)     ( (ushort*)(p) )[1]

#define __pos_of_b_block(p)     ( (uint*)(p) )[1]

#define __size_of_block(p)      ( __is_big_block(p) )               \
                                ? ( *( (uint*)(p) ) ^ 0x80000000 )  \
                                : *( (ushort*)(p) )

#define __pos_of_block(p)       ( __is_big_block(p) )   \
                                ? __pos_of_b_block(p)   \
                                : __pos_of_s_block(p)

#define __mv_fw_block(ptr, delta)                   \
    if ( __is_big_block(ptr) ) {                    \
        ( (uint*)(ptr) )[1] += (delta);             \
    } else {                                        \
        ( (ushort*)(ptr) )[1] += (ushort)(delta);   \
    }

#define __set_block_pos(ptr, pos)               \
    if( __is_big_block(ptr) ){                  \
        ( (uint*)(ptr) )[1] = (pos);            \
    } else {                                    \
        ( (ushort*)(ptr) )[1] = (ushort)(pos);  \
    }

#define __data_of_block(p)                                                  \
    ( __is_big_block(p) )                                                   \
    ? (byte*)( ( (byte*)( ((uint*)(p))   + 2 ) ) + __pos_of_b_block(p) )    \
    : (byte*)( ( (byte*)( ((ushort*)(p)) + 2 ) ) + __pos_of_s_block(p) )

#define __entire_data_of_block(p)               \
    ( __is_big_block(p) )                       \
    ? (byte*)( (byte*)( ((uint*)(p))   + 2 ) )  \
    : (byte*)( (byte*)( ((ushort*)(p)) + 2 ) )

namespace SDN_Raft {

static void free_Buffer(Buffer* buf) {
    static stat_elem& num_active = *stat_mgr::get_instance()->create_stat
        (stat_elem::COUNTER, "num_active_Buffers");
    static stat_elem& amount_active = *stat_mgr::get_instance()->create_stat
        (stat_elem::COUNTER, "amount_active_Buffers");

    num_active--;
    amount_active -= buf->container_size();

    delete[] reinterpret_cast<char*>(buf);
}

ptr<Buffer> Buffer::alloc(const size_t size) {
    static stat_elem& num_allocs = *stat_mgr::get_instance()->create_stat
        (stat_elem::COUNTER, "num_Buffer_allocs");
    static stat_elem& amount_allocs = *stat_mgr::get_instance()->create_stat
        (stat_elem::COUNTER, "amount_Buffer_allocs");
    static stat_elem& num_active = *stat_mgr::get_instance()->create_stat
        (stat_elem::COUNTER, "num_active_Buffers");
    static stat_elem& amount_active = *stat_mgr::get_instance()->create_stat
        (stat_elem::COUNTER, "amount_active_Buffers");

    if (size >= 0x80000000) {
        throw std::out_of_range( "size exceed the max size that "
                                 "SDN_Raft::Buffer could support" );
    }
    num_allocs++;
    num_active++;

    if (size >= 0x8000) {
        size_t len = size + sizeof(uint) * 2;
        ptr<Buffer> buf( reinterpret_cast<Buffer*>(new char[len]),
                         &free_Buffer );
        amount_allocs += len;
        amount_active += len;

        any_ptr ptr = reinterpret_cast<any_ptr>( buf.get() );
        __init_b_block(ptr, size);
        return buf;
    }

    size_t len = size + sizeof(ushort) * 2;
    ptr<Buffer> buf( reinterpret_cast<Buffer*>(new char[len]),
                     &free_Buffer );
    amount_allocs += len;
    amount_active += len;

    any_ptr ptr = reinterpret_cast<any_ptr>( buf.get() );
    __init_s_block(ptr, size);

    return buf;
}

ptr<Buffer> Buffer::copy(const Buffer& buf) {
    ptr<Buffer> other = alloc(buf.size() - buf.pos());
    other->put(buf);
    other->pos(0);
    return other;
}

ptr<Buffer> Buffer::clone(const Buffer& buf) 
{
    ptr<Buffer> other = alloc(buf.size());

    byte* dst = other->data_begin();
    byte* src = buf.data_begin();
    ::memcpy(dst, src, buf.size());

    other->pos(0);
    return other;
}

ptr<Buffer> Buffer::expand(const Buffer& buf, uint32_t new_size) {
     if (new_size >= 0x80000000) {
        throw std::out_of_range( "size exceed the max size that "
                                 "SDN_Raft::Buffer could support" );
    }

    if (new_size < buf.size()) {
        throw std::out_of_range( "realloc() new_size is less than "
                                 "old size" );
    }
    ptr<Buffer> other = alloc(new_size);
    byte* dst = other->data_begin();
    byte* src = buf.data_begin();
    ::memcpy(dst, src, buf.size());
    other->pos(buf.pos());
    return other;
}

size_t Buffer::container_size() const {
    return (size_t)( __size_of_block(this) +
                     ( ( __is_big_block(this) )
                       ? sizeof(uint) * 2
                       : sizeof(ushort) * 2 ) );
}

size_t Buffer::size() const {
    return (size_t)( __size_of_block(this) );
}

size_t Buffer::pos() const {
    return (size_t)( __pos_of_block(this) );
}

byte* Buffer::data() const {
    return __data_of_block(this);
}

byte* Buffer::data_begin() const {
    return __entire_data_of_block(this);
}

int32 Buffer::get_int() {
    size_t avail = size() - pos();
    if (avail < sz_int) {
        throw std::overflow_error
              ( "insufficient Buffer available for an int32 value" );
    }

    byte* d = data();
    int32 val = 0;
    for (size_t i = 0; i < sz_int; ++i) {
        int32 byte_val = (int32)*(d + i);
        val += (byte_val << (i * 8));
    }

    __mv_fw_block(this, sz_int);
    return val;
}

ulong Buffer::get_ulong() {
    size_t avail = size() - pos();
    if (avail < sz_ulong) {
        throw std::overflow_error
              ( "insufficient Buffer available for an ulong value" );
    }

    byte* d = data();
    ulong val = 0L;
    for (size_t i = 0; i < sz_ulong; ++i) {
        ulong byte_val = (ulong)*(d + i);
        val += (byte_val << (i * 8));
    }

    __mv_fw_block(this, sz_ulong);
    return val;
}

byte Buffer::get_byte() {
    size_t avail = size() - pos();
    if (avail < sz_byte) {
        throw std::overflow_error
              ( "insufficient Buffer available for a byte" );
    }

    byte val = *data();
    __mv_fw_block(this, sz_byte);
    return val;
}

const byte* Buffer::get_bytes(size_t& len)
{
    size_t avail = size() - pos();
    if (avail < sz_int) {
        throw std::overflow_error
              ( "insufficient Buffer available for a bytes length (int32)" );
    }

    byte* d = data();
    int32 val = 0;
    for (size_t i = 0; i < sz_int; ++i) {
        int32 byte_val = (int32)*(d + i);
        val += (byte_val << (i * 8));
    }

    __mv_fw_block(this, sz_int);
    len = val;

    d = data();
    if ( size() - pos() < len) {
        throw std::overflow_error
              ( "insufficient Buffer available for a byte array" );
    }

    __mv_fw_block(this, len);
    return reinterpret_cast<const byte*>(d);
}

void Buffer::pos(size_t p) {
    size_t position = ( p > size() ) ? size() : p;
    __set_block_pos(this, position);
}

const char* Buffer::get_str() {
    size_t p = pos();
    size_t s = size();
    size_t i = 0;
    byte* d = data();
    while ( (p + i) < s && *(d + i) ) ++i;
    if (i == 0) {
        // Empty string, move forward 1 byte for NULL character.
        __mv_fw_block(this, i + 1);
    }
    if (p + i >= s || i == 0) {
        return nilptr;
    }

    __mv_fw_block(this, i + 1);
    return reinterpret_cast<const char*>(d);
}

void Buffer::put(byte b) {
    if (size() - pos() < sz_byte) {
        throw std::overflow_error("insufficient Buffer to store byte");
    }

    byte* d = data();
    *d = b;
    __mv_fw_block(this, sz_byte);
}

void Buffer::put(const char* ba, size_t len) {
    put( (const byte*)ba, len );
}

void Buffer::put(const byte* ba, size_t len)
{
    // put length as int32 first
    if (size() - pos() < sz_int) {
        throw std::overflow_error("insufficient Buffer to store int32 length");
    }

    byte* d = data();
    for (size_t i = 0; i < sz_int; ++i) {
        *(d + i) = (byte)(len >> (i * 8));
    }

    __mv_fw_block(this, sz_int);

    // put bytes
    put_raw(ba, len);
}

void Buffer::put(int32 val) {
    if (size() - pos() < sz_int) {
        throw std::overflow_error("insufficient Buffer to store int32");
    }

    byte* d = data();
    for (size_t i = 0; i < sz_int; ++i) {
        *(d + i) = (byte)(val >> (i * 8));
    }

    __mv_fw_block(this, sz_int);
}

void Buffer::put(ulong val) {
    if (size() - pos() < sz_ulong) {
        throw std::overflow_error("insufficient Buffer to store unsigned long");
    }

    byte* d = data();
    for (size_t i = 0; i < sz_ulong; ++i) {
        *(d + i) = (byte)(val >> (i * 8));
    }

    __mv_fw_block(this, sz_ulong);
}

void Buffer::put(const std::string& str) {
    if (size() - pos() < (str.length() + 1)) {
        throw std::overflow_error("insufficient Buffer to store a string");
    }

    byte* d = data();
    for (size_t i = 0; i < str.length(); ++i) {
        *(d + i) = (byte)str[i];
    }

    *(d + str.length()) = (byte)0;
    __mv_fw_block(this, str.length() + 1);
}

void Buffer::get(ptr<Buffer>& dst) {
    size_t sz = dst->size() - dst->pos();
    ::memcpy(dst->data(), data(), sz);
    __mv_fw_block(this, sz);
}

void Buffer::put(const Buffer& buf) {
    size_t sz = size();
    size_t p = pos();
    size_t src_sz = buf.size();
    size_t src_p = buf.pos();
    if ((sz - p) < (src_sz - src_p)) {
        throw std::overflow_error
              ( "insufficient Buffer to hold the other Buffer" );
    }

    byte* d = data();
    byte* src = buf.data();
    ::memcpy(d, src, src_sz - src_p);
    __mv_fw_block(this, src_sz - src_p);
}

byte* Buffer::get_raw(size_t len) {
    if ( size() - pos() < len) {
        throw std::overflow_error
              ( "insufficient Buffer available for a raw byte array" );
    }

    byte* d = data();
    __mv_fw_block(this, len);
    return d;
}

void Buffer::put_raw(const byte* ba, size_t len) {
    if ( size() - pos() < len) {
        throw std::overflow_error
              ( "insufficient Buffer to store a raw byte array" );
    }

    ::memcpy(data(), ba, len);
    __mv_fw_block(this, len);
}

}  // namespace SDN_Raft;
using namespace SDN_Raft;

std::ostream& SDN_Raft::operator << (std::ostream& out, Buffer& buf) {
    if (!out) {
        throw std::ios::failure("bad output stream.");
    }

    out.write(reinterpret_cast<char*>(buf.data()), buf.size() - buf.pos());

    if (!out) {
        throw std::ios::failure("write failed");
    }

    return out;
}

std::istream& SDN_Raft::operator >> (std::istream& in, Buffer& buf) {
    if (!in) {
        throw std::ios::failure("bad input stream");
    }

    char* data = reinterpret_cast<char*>(buf.data());
    int size = buf.size() - buf.pos();
    in.read(data, size);

    if (!in) {
        throw std::ios::failure("read failed");
    }

    return in;
}

