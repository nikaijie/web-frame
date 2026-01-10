#pragma once
#include <memory>
#include <atomic>
#include <chrono>
#include "data_structure/channel.h"

namespace runtime {

    class Context {
    public:
        using Ptr = std::shared_ptr<Context>;

        Context() : done_chan_(std::make_shared<Channel<int>>(1)), done_flag_(false) {}

        // 模擬 context.WithTimeout
        static Ptr WithTimeout(int timeout_ms);

        // 檢查是否已超時/取消
        bool is_done() const { return done_flag_.load(); }

        // 獲取信號 Channel (對標 Go 的 <-ctx.Done())
        std::shared_ptr<Channel<int>> done() { return done_chan_; }

        // 手動觸發取消
        void cancel() {
            if (!done_flag_.exchange(true)) {
                done_chan_->push(1); // 發送結束信號
            }
        }

    private:
        std::shared_ptr<Channel<int>> done_chan_;
        std::atomic<bool> done_flag_;
    };

} // namespace runtime