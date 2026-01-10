#include "runtime/scheduler.h"
#include "runtime/netpoller.h"
#include <iostream>

namespace runtime {

Scheduler& Scheduler::get() {
    static Scheduler instance;
    return instance;
}

Scheduler::~Scheduler() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
}

void Scheduler::start(size_t thread_count) {
    std::thread io_thread([]() {
        Netpoller::get().poll_loop();
    });
    io_thread.detach();

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

// 核心：注册定时器，cb为回调函数
void Scheduler::add_timer(int ms, Goroutine::Ptr g, std::function<void()> cb) {
    auto expires = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    {
        std::lock_guard<std::mutex> lock(timer_mutex_);
        timers_.push({expires, std::move(g), std::move(cb)});
    }
    cv_.notify_all();
}

// 核心：检查并唤醒到期协程
void Scheduler::check_timers() {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    auto now = std::chrono::steady_clock::now();
    while (!timers_.empty() && timers_.top().expires_at <= now) {
        Timer t = std::move(const_cast<Timer&>(timers_.top()));
        timers_.pop();

        if (t.callback) t.callback(); // 如果有回调（如 Context 取消）则执行
        if (t.g) push_ready(t.g);     // 重新放入就绪队列
    }
}

void Scheduler::worker_loop() {
    while (true) {
        check_timers(); // 每次循环开始先查一下闹钟

        Goroutine::Ptr g;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // 防止worker线程因为条件变量不能唤醒，有超时
            auto wait_timeout = std::chrono::milliseconds(10);
            cv_.wait_for(lock, wait_timeout, [this] {
                return !ready_queue_.empty() || stop_;
            });

            if (stop_ && ready_queue_.empty()) break;
            if (ready_queue_.empty()) continue;

            g = std::move(ready_queue_.front());
            ready_queue_.pop();
        }

        if (g) {
            g->resume();
        }
    }
}

// 真正的协程 Sleep 现身！
void sleep(int ms) {
    auto g = Goroutine::current();
    if (!g) return;
    Scheduler::get().add_timer(ms, g);
    // 立即让出 CPU
    Goroutine::yield();
}

void go(Goroutine::Task task) {
    auto g = std::make_shared<Goroutine>(std::move(task));
    Scheduler::get().push_ready(g);
}

} // namespace runtime