#include "../../include/data_structure/co_mutex.h"
#include "runtime/scheduler.h"

namespace runtime {

    void CoMutex::lock() {
        if (try_lock()) {
            return; // 抢锁成功，直接返回执行业务
        }


        auto current_g = Goroutine::current();
        {
            wait_queue_lock_.lock();
            waiting_gs_.push(current_g);
            wait_queue_lock_.unlock();
        }

        // 让出 CPU 权限，协程在此暂停
        Goroutine::yield();

        // --- 协程被唤醒后从这里继续执行 ---
        // 注意：这里的唤醒机制通常保证了唤醒即持锁，或者需要再次尝试
    }

    void CoMutex::unlock() {
        Goroutine::Ptr next_g = nullptr;

        {
            wait_queue_lock_.lock();
            if (!waiting_gs_.empty()) {
                next_g = waiting_gs_.front();
                waiting_gs_.pop();
            } else {
                locked_.store(false); // 没人等，直接释放
            }
            wait_queue_lock_.unlock();
        }

        if (next_g) {
            Scheduler::get().push_ready(next_g);
        }
    }

    bool CoMutex::try_lock() {
        bool expected = false;
        return locked_.compare_exchange_strong(expected, true);
    }

} // namespace runtime