
#ifndef  _TRACER_HXX
#define  _TRACER_HXX

#include "Logger.hxx"
#include <inttypes.h>
#include <string>

#include <stdarg.h>


static inline std::string msg_if_given(const char* format, ...)
    __attribute__((format(printf, 1, 2)));

static inline std::string msg_if_given(const char* format, ...) {
    if (format[0] == 0x0) {
        return "";
    }
    std::string msg(2048, '\0');
    for (int i = 0; i < 2; ++i) {
        va_list args;
        va_start(args, format);
        const int len = vsnprintf(&msg[0], msg.size(), format, args);
        va_end(args);
        if (len < 0) {
            return std::string("invalid format ") + format;
        }
        if (static_cast<size_t>(len) < msg.size()) {
            msg.resize(len);
            break;
        }
        msg.resize(len + 1);
    }

    // Get rid of newline at the end.
    if ((not msg.empty()) && (msg.back() == '\n')) {
        msg.pop_back();
    }
    return msg;
}

#define L_TRACE (6)
#define L_DEBUG (5)
#define L_INFO  (4)
#define L_WARN  (3)
#define L_ERROR (2)
#define L_FATAL (1)

#define output(lv, ...) \
    if (l_ && l_->get_level() >= (lv)) \
        l_->put_details((lv), __FILE__, __func__, __LINE__, msg_if_given(__VA_ARGS__))

// trace.
#define output_trace(...) \
    if (l_ && l_->get_level() >= 6) \
        l_->put_details(6, __FILE__, __func__, __LINE__, msg_if_given(__VA_ARGS__))

// debug verbose.
#define output_verbose(...) \
    if (l_ && l_->get_level() >= 5) \
        l_->put_details(5, __FILE__, __func__, __LINE__, msg_if_given(__VA_ARGS__))

// debug.
#define output_debug(...) \
    if (l_ && l_->get_level() >= 5) \
        l_->put_details(5, __FILE__, __func__, __LINE__, msg_if_given(__VA_ARGS__))

// info.
#define output_info(...) \
    if (l_ && l_->get_level() >= 4) \
        l_->put_details(4, __FILE__, __func__, __LINE__, msg_if_given(__VA_ARGS__))

// warning.
#define output_warn(...) \
    if (l_ && l_->get_level() >= 3) \
        l_->put_details(3, __FILE__, __func__, __LINE__, msg_if_given(__VA_ARGS__))

// error.
#define p_er(...) \
    if (l_ && l_->get_level() >= 2) \
        l_->put_details(2, __FILE__, __func__, __LINE__, msg_if_given(__VA_ARGS__))

// fatal.
#define output_fetal(...) \
    if (l_ && l_->get_level() >= 1) \
        l_->put_details(1, __FILE__, __func__, __LINE__, msg_if_given(__VA_ARGS__))

#endif