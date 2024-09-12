#ifndef _BUFFER_HXX_
#define _BUFFER_HXX_

#include "util.hxx"
#include "ptr.hxx"

#include <string>

namespace SDN_Raft {

class Buffer {
    Buffer() = delete;
    __nocopy__(Buffer);

public:
    /*操作Buffer内存空间*/
    static ptr<Buffer> alloc(const size_t size);
    static ptr<Buffer> copy(const Buffer& buf);   
    static ptr<Buffer> clone(const Buffer& buf);
    static ptr<Buffer> expand(const Buffer& buf, uint32_t new_size);

    /*获取元数据信息*/
    size_t container_size() const;
    size_t size() const;
    size_t pos() const;
    void pos(size_t p);

    /*读取Buffer数据*/
    byte* data() const;
    byte* data_begin() const;
    int32 get_int();
    ulong get_ulong();
    byte get_byte();
    const byte* get_bytes(size_t& len);
    void get(ptr<Buffer>& dst);
    const char* get_str();
    byte* get_raw(size_t len);

    /*写入Buffer数据*/
    void put(byte b);
    void put(const char* ba, size_t len);
    void put(const byte* ba, size_t len);
    void put(int32 val);
    void put(ulong val);
    void put(const std::string& str);
    void put(const Buffer& buf);
    void put_raw(const byte* ba, size_t len);
};

std::ostream& operator << (std::ostream& out, Buffer& buf);
std::istream& operator >> (std::istream& in, Buffer& buf);

class BufferSerializer {
public:
    enum endianness {
        LITTLE = 0x0,
        BIG = 0x1,
    };

    BufferSerializer(Buffer& src_buf,
                      endianness endian = LITTLE);

    BufferSerializer(ptr<Buffer>& src_buf_ptr,
                      endianness endian = LITTLE);

    __nocopy__(BufferSerializer);

public:

    inline size_t pos() const { return pos_; }
    size_t size() const;
    void pos(size_t new_pos);
    void* data() const;

   
    void put_u8(uint8_t val);
    void put_u16(uint16_t val);
    void put_u32(uint32_t val);
    void put_u64(uint64_t val);
    void put_i8(int8_t val);
    void put_i16(int16_t val);
    void put_i32(int32_t val);
    void put_i64(int64_t val);
    void put_raw(const void* raw_ptr, size_t len);
    void put_buffer(const Buffer& buf);
    void put_bytes(const void* raw_ptr, size_t len);
    void put_str(const std::string& str);
    void put_cstr(const char* str);

   
    uint8_t get_u8();
    uint16_t get_u16();
    uint32_t get_u32();
    uint64_t get_u64();
    int8_t get_i8();
    int16_t get_i16();
    int32_t get_i32();
    int64_t get_i64();
    void* get_raw(size_t len);
    void get_buffer(ptr<Buffer>& dst);
    void* get_bytes(size_t& len);
    std::string get_str();
    const char* get_cstr();

private:
    bool is_valid(size_t len) const;

    /*字节序*/
    endianness endian_;

    /*序列化的缓存*/
    Buffer& buf_;

    size_t pos_;
};

}
#endif //_BUFFER_HXX_
