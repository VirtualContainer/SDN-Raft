#ifndef _LOGGER_HXX_
#define _LOGGER_HXX_

#include "util.hxx"

#include <string>

namespace SDN_Raft {

class Logger {
    __interface_body__(Logger);

public:
    virtual void debug(const std::string& log_line) {}

    virtual void info(const std::string& log_line) {}

    virtual void warn(const std::string& log_line) {}

    virtual void err(const std::string& log_line) {}

    virtual void set_level(int l) {}

    virtual int  get_level() { return 6; }

    virtual void put_details(int level,
                             const char* source_file,
                             const char* func_name,
                             size_t line_number,
                             const std::string& log_line) {}
};

}

#endif 
