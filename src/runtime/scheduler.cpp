#include "runtime/scheduler.h"
#include "runtime/netpoller.h"
#include <iostream>

namespace runtime {

// 单例模式实现
Scheduler& Scheduler::get() {
    static Scheduler instance;
    return instance;
}

// 析构函数：负责优雅停止所有线程
Scheduler::~Scheduler() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all(); // 唤醒所有在等待任务的 Worker

    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void Scheduler::start(size_t thread_count) {
    // 1. 启动 Netpoller IO 线程
    // 我们让 IO 线程独立于 Worker 运行，专门负责监听 kqueue 事件
    std::thread io_thread([]() {
        Netpoller::get().poll_loop();
    });
    io_thread.detach(); // 后台运行，生命周期随进程

    // 2. 启动指定数量的 Worker 线程
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&Scheduler::worker_loop, this);
    }
}

void Scheduler::push_ready(Goroutine::Ptr g) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ready_queue_.push(std::move(g));
    }
    cv_.notify_one(); // 通知一个空闲的 Worker 来领任务
}

void Scheduler::worker_loop() {
    while (true) {
        Goroutine::Ptr g;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // 如果队列为空且没有停止信号，就阻塞等待
            cv_.wait(lock, [this] { return !ready_queue_.empty() || stop_; });

            if (stop_ && ready_queue_.empty()) {
                break;
            }

            g = std::move(ready_queue_.front());

            ready_queue_.pop();
        }

        if (g) {
            // 物理线程切换到协程栈执行业务代码
            g->resume();

            // resume 返回后有两种情况：
            // 1. 协程运行完了 (is_finished == true)
            // 2. 协程通过 yield 主动挂起了 (比如在等 IO)

            if (g->is_finished()) {
                // 协程已结束，shared_ptr 会自动释放相关内存
            } else {
                // 如果协程是 yield 出来的，我们不需要在这里把它放回 ready_queue_。
                // 因为如果是 IO 阻塞，Netpoller 会在数据就绪后将其重新放入队列。
                // 此时 Worker 线程只需去领下一个任务即可。
            }
        }
    }
}

// 模拟 Go 语言的 go 关键字
void go(Goroutine::Task task) {
    auto g = std::make_shared<Goroutine>(std::move(task));
    Scheduler::get().push_ready(g);
}

} // namespace runtime