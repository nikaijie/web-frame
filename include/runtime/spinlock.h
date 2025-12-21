#pragma once
#include <atomic>

namespace runtime {
    class Spinlock {
    public:
        Spinlock() : flag_(ATOMIC_FLAG_INIT) {
        }

        /**
         * @brief 尝试加锁。
         * @return true 成功获取锁；false 20次自旋后仍未获取。
         */
        inline bool lock() {
            for (int i = 0; i < 20; ++i) {
                if (!flag_.test_and_set(std::memory_order_acquire)) {
                    return true;
                }
#if defined(__i386__) || defined(__x86_64__)
                __builtin_ia32_pause();
#endif
            }
            return false; // 20次没拿到，不阻塞线程，直接返回
        }

        inline void unlock() {
            flag_.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag flag_;
    };
} // namespace runtime
