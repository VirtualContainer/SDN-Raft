#ifndef _TIMER_TASK_HXX_
#define _TIMER_TASK_HXX_

#include "delayed_task.hxx"

namespace SDN_Raft {

enum timer_task_type {
    election_timer = 0x1,
    heartbeat_timer = 0x2,
};

template<typename T>
class timer_task : public delayed_task {
public:
    typedef std::function<void(T)> executor;

    timer_task(executor& e, T ctx, int32 type = 0)
        : delayed_task(type), exec_(e), ctx_(ctx) {}
protected:
    virtual void exec() __override__ {
        if (exec_) {
            exec_(ctx_);
        }
    }
private:
    executor exec_;
    T ctx_;
};

template<>
class timer_task<void> : public delayed_task {
public:
    typedef std::function<void()> executor;

    explicit timer_task(executor& e, int32 type = 0)
        : delayed_task(type)
        , exec_(e) {}
protected:
    virtual void exec() __override__ {
        if (exec_) {
            exec_();
        }
    }
private:
    executor exec_;
};

}

#endif //_TIMER_TASK_HXX_
