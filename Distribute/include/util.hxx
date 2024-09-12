#ifndef _PP_UTIL_HXX_
#define _PP_UTIL_HXX_


#define __override__ override

#define __nocopy__(clazz)                       \
    private:                                    \
    clazz(const clazz&) = delete;               \
    clazz& operator=(const clazz&) = delete;    \

#define nilptr nullptr

#define __interface_body__(clazz)   \
    public:                         \
    clazz(){}                       \
    virtual ~clazz() {}             \
    __nocopy__(clazz)

#define auto_lock(lock)     std::lock_guard<std::mutex> guard(lock)
#define recur_lock(lock)    std::unique_lock<std::recursive_mutex> guard(lock)

#define sz_int      sizeof(int32)
#define sz_ulong    sizeof(ulong)
#define sz_byte     sizeof(byte)


#include <cstdint>
namespace SDN_Raft {

typedef uint64_t ulong;
typedef int64_t int64;
typedef void* any_ptr;
typedef unsigned char byte;
typedef uint16_t ushort;
typedef uint32_t uint;
typedef int32_t int32;

}


#endif //_PP_UTIL_HXX_

