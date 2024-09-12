#ifndef __EVENT_AWAITER_HXX__
#define __EVENT_AWAITER_HXX__

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace SDN_Raft {

class EventAwaiter {
private:
    enum class AS {
        idle    = 0x0,
        ready   = 0x1,
        waiting = 0x2,
        done    = 0x3
    };

public:
    EventAwaiter() : status(AS::idle) {}

    void reset() {
        status.store(AS::idle);
    }

    void wait() {
        wait_us(0);
    }

    void wait_ms(size_t time_ms) {
        wait_us(time_ms * 1000);
    }

    void wait_us(size_t time_us) {
        AS expected = AS::idle;
        if (status.compare_exchange_strong(expected, AS::ready)) {
            std::unique_lock<std::mutex> l(cvLock);
            expected = AS::ready;
            if (status.compare_exchange_strong(expected, AS::waiting)) {
                if (time_us) {
                    cv.wait_for(l, std::chrono::microseconds(time_us));
                } else {
                    cv.wait(l);
                }
                status.store(AS::done);
            } else {
                
            }
        } else {
            
        }
    }

    void invoke() {
        AS expected = AS::idle;
        if (status.compare_exchange_strong(expected, AS::done)) {
            // wait() has not been invoked yet, do nothing.
            return;
        }

        std::unique_lock<std::mutex> l(cvLock);
        expected = AS::ready;
        if (status.compare_exchange_strong(expected, AS::done)) {
            // wait() has been called earlier than invoke(),
            // but invoke() has grabbed `cvLock` earlier than wait().
            // Do nothing.
        } else {
            // wait() is waiting for ack.
            cv.notify_all();
        }
    }

private:
    std::atomic<AS> status;
    std::mutex cvLock;
    std::condition_variable cv;
};

}

#endif