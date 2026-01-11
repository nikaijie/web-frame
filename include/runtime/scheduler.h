#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include "runtime/goroutine.h"

namespace runtime {

    // 定时器结构
    struct Timer {
        std::chrono::steady_clock::time_point expires_at;
        Goroutine::Ptr g;
        std::function<void()> callback; // 可选：支持超时后的自定义回调

        // 最小堆：时间早的在前
        bool operator>(const Timer& other) const {
            return expires_at > other.expires_at;
        }
    };

    class Scheduler {
    public:
        static Scheduler& get();
        void start(size_t thread_count = std::thread::hardware_concurrency());
        void push_ready(Goroutine::Ptr g);

        // 核心：添加定时器
        void add_timer(int ms, Goroutine::Ptr g, std::function<void()> cb = nullptr);

        ~Scheduler();

    private:
        Scheduler() = default;
        void worker_loop();
        void check_timers(); // 检查是否有协程该起床了

        std::vector<std::thread> workers_;
        std::queue<Goroutine::Ptr> ready_queue_;

        // 定时器相关
        std::priority_queue<Timer, std::vector<Timer>, std::greater<Timer>> timers_;
        std::mutex timer_mutex_; // 定时器专用锁，减少对主队列锁的竞争

        std::mutex mutex_;
        std::condition_variable cv_;
        bool stop_ = false;
    };

    void go(Goroutine::Task task);
    void sleep(int ms); // 协程版 sleep 声明

} // namespace runtime