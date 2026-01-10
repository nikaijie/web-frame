#pragma once
#include <atomic>
#include <vector>
#include "runtime/goroutine.h"
#include "runtime/spinlock.h"

namespace runtime {

    class WaitGroup {
    public:
        WaitGroup() : counter_(0) {}
        ~WaitGroup() = default;

        // 禁止拷贝
        WaitGroup(const WaitGroup&) = delete;
        WaitGroup& operator=(const WaitGroup&) = delete;

        // 增加/减少计数
        void add(int delta);

        // 完成一个任务
        void done();

        // 挂起当前协程，直到计数器归零
        void wait();

    private:
        std::atomic<int> counter_;
        runtime::Spinlock lock_;
        std::vector<runtime::Goroutine::Ptr> waiting_gs_;
    };

} // namespace runtime