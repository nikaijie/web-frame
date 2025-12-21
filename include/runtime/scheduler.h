#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "runtime/goroutine.h"

namespace runtime {

    class Scheduler {
    public:
        // 获取全局单例
        static Scheduler& get();

        // 启动调度器：创建 n 个物理线程
        void start(size_t thread_count = std::thread::hardware_concurrency());

        // 将协程放入就绪队列
        void push_ready(Goroutine::Ptr g);

        ~Scheduler();

    private:
        Scheduler() = default;
        void worker_loop(); // Worker 线程的核心循环

        std::vector<std::thread> workers_;
        std::queue<Goroutine::Ptr> ready_queue_; // 暂时用 mutex 保护，后期可优化为 lock-free
        std::mutex mutex_;
        std::condition_variable cv_;
        bool stop_ = false;
    };

    // 全局 go 函数的真正实现
    void go(Goroutine::Task task);

} // namespace runtime