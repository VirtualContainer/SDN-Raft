#ifndef _DELAYED_TASK_HXX_
#define _DELAYED_TASK_HXX_

#include "util.hxx"

#include <atomic>
#include <functional>

namespace SDN_Raft {

class delayed_task {
public:
    delayed_task(int32 type = 0)
        : cancelled_(false)
        , impl_ctx_(nilptr)
        , impl_ctx_del_()
        , type_(type)
        {}

    virtual ~delayed_task() {
        if (impl_ctx_ != nilptr) {
            if (impl_ctx_del_) {
                impl_ctx_del_(impl_ctx_);
            }
        }
    }

    __nocopy__(delayed_task);

public:
    void execute() {
        if (!cancelled_.load()) {
            exec();
        }
    }

    void cancel() {
        cancelled_.store(true);
    }

    void reset() {
        cancelled_.store(false);
    }

    void* get_impl_context() const {
        return impl_ctx_;
    }

    void set_impl_context(void* ctx, std::function<void(void*)> del) {
        impl_ctx_ = ctx;
        impl_ctx_del_ = del;
    }

    int32 get_type() const {
        return type_;
    }

protected:
    virtual void exec() = 0;

private:
    std::atomic<bool> cancelled_;
    void* impl_ctx_;
    std::function<void(void*)> impl_ctx_del_;
    int32 type_;
};

}

#endif
