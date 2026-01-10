#pragma once
#include <queue>
#include <memory>
#include <mutex>
#include "runtime/goroutine.h"
#include "runtime/spinlock.h"
#include "runtime/scheduler.h"

namespace runtime {

template<typename T>
class Channel {
public:
    explicit Channel(size_t capacity = 0) : capacity_(capacity) {}

    // 禁止拷貝，防止鎖和佇列狀態混亂
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    /**
     * 發送數據
     */
    void push(T value) {
        while (true) {
            Goroutine::Ptr g_to_wake = nullptr;
            {
                // 1. 使用自旋鎖保護內部數據結構（極短時間）
                lock_.lock();

                // 檢查是否有正在等待接收的協程
                if (!recv_waiters_.empty()) {
                    buffer_.push(std::move(value));
                    g_to_wake = recv_waiters_.front();
                    recv_waiters_.pop();
                    lock_.unlock();

                    // 喚醒接收者，並直接返回
                    if (g_to_wake) Scheduler::get().push_ready(g_to_wake);
                    return;
                }

                // 如果緩衝區未滿，直接入隊
                if (buffer_.size() < capacity_ || (capacity_ == 0 && buffer_.empty())) {
                    buffer_.push(std::move(value));
                    lock_.unlock();
                    return;
                }

                // 緩衝區滿了，記錄當前協程，準備掛起
                send_waiters_.push(Goroutine::current());
                lock_.unlock();
            }

            // 2. 釋放自旋鎖後，執行協程切換（這才是真正的阻塞）
            Goroutine::yield();
        }
    }

    /**
     * 接收數據
     */
    T pop() {
        while (true) {
            Goroutine::Ptr g_to_wake = nullptr;
            {
                lock_.lock();

                // 如果緩衝區有數據，直接取出
                if (!buffer_.empty()) {
                    T val = std::move(buffer_.front());
                    buffer_.pop();

                    // 取出數據後，緩衝區有了空位，嘗試喚醒一個發送者
                    if (!send_waiters_.empty()) {
                        g_to_wake = send_waiters_.front();
                        send_waiters_.pop();
                    }
                    lock_.unlock();

                    if (g_to_wake) Scheduler::get().push_ready(g_to_wake);
                    return val;
                }

                // 緩衝區空了，記錄當前接收協程，準備掛起
                recv_waiters_.push(Goroutine::current());
                lock_.unlock();
            }

            // 釋放鎖後掛起，等待 push 操作喚醒
            Goroutine::yield();
        }
    }

private:
    size_t capacity_;
    std::queue<T> buffer_;

    // 自旋鎖：只負責保護 std::queue 的多執行緒併發安全
    Spinlock lock_;

    // 協程等待佇列
    std::queue<Goroutine::Ptr> send_waiters_;
    std::queue<Goroutine::Ptr> recv_waiters_;
};

}