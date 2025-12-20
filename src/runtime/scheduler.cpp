#include "../../include/runtime/scheduler.h"

namespace runtime {

    Scheduler& Scheduler::get() {
        static Scheduler instance;
        return instance;
    }

    void Scheduler::start(size_t thread_count) {
        for (size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back(&Scheduler::worker_loop, this);
        }
    }

    void Scheduler::push_ready(Goroutine::Ptr g) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ready_queue_.push(std::move(g));
        }
        cv_.notify_one();
    }

    void Scheduler::worker_loop() {
        while (!stop_) {
            Goroutine::Ptr g;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !ready_queue_.empty() || stop_; });
                if (stop_) break;
                g = std::move(ready_queue_.front());
                ready_queue_.pop();
            }

            if (g) {
                g->resume(); // 物理线程跳进协程执行

                // 关键：检查协程是否没跑完（比如 yield 出来了）
                if (!g->is_finished()) {
                    // 如果是因为 IO 还没到，目前我们先手动把它塞回队列模拟
                    // 以后这里会交给 Netpoller (kqueue)
                    push_ready(std::move(g));
                }
            }
        }
    }

    void go(Goroutine::Task task) {
        auto g = std::make_shared<Goroutine>(std::move(task));
        Scheduler::get().push_ready(std::move(g));
    }

} // namespace runtime