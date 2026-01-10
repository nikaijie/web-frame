#pragma once
#include <queue>
#include <atomic>
#include <mutex>
#include "runtime/goroutine.h"
#include "runtime/spinlock.h"

namespace runtime {

    class CoMutex {
    public:
        CoMutex() : locked_(false) {}

        // 禁止拷贝
        CoMutex(const CoMutex&) = delete;

        void lock();
        void unlock();
        bool try_lock();

    private:
        std::atomic<bool> locked_;
        // 保护等待队列的轻量级锁
        runtime::Spinlock wait_queue_lock_;
        // 存储等待唤醒的协程
        std::queue<Goroutine::Ptr> waiting_gs_;
    };

} // namespace runtime