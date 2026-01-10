#include "../include/data_structure/wait_group.h"
#include "runtime/scheduler.h"

namespace runtime {

    void WaitGroup::add(int delta) {
        int val = (counter_ += delta);

        if (val < 0) {
            throw std::runtime_error("WaitGroup counter negative");
        }

        // 如果计数器归零，唤醒所有正在 Wait 的协程
        if (val == 0) {
            std::vector<runtime::Goroutine::Ptr> to_wake;
            {
                lock_.lock();
                to_wake.swap(waiting_gs_); // 快速交换并释放锁
                lock_.unlock();
            }

            for (auto& g : to_wake) {
                Scheduler::get().push_ready(std::move(g));
            }
        }
    }

    void WaitGroup::done() {
        add(-1);
    }

    void WaitGroup::wait() {
        if (counter_.load() == 0) {
            return;
        }

        {
            lock_.lock();
            // Double check: 拿锁后再检查一遍，防止在拿锁期间 counter 归零了
            if (counter_.load() == 0) {
                lock_.unlock();
                return;
            }
            // 将当前协程加入等待列表
            waiting_gs_.push_back(runtime::Goroutine::current());
            lock_.unlock();
        }

        // 核心：让出 CPU，直到被 Add(val==0) 时唤醒
        runtime::Goroutine::yield();
    }

} // namespace runtime