#pragma once
#include <atomic>

namespace runtime {
    class Spinlock {
    public:
        Spinlock() : flag_(ATOMIC_FLAG_INIT) {
        }


        inline void lock() {
            while (flag_.test_and_set(std::memory_order_acquire)) {
#if defined(__i386__) || defined(__x86_64__)
                __builtin_ia32_pause();
#elif defined(__arm__) || defined(__aarch64__)
                asm volatile("yield");
#endif
            }
        }

        inline void unlock() {
            flag_.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag flag_;
    };
} // namespace runtime
