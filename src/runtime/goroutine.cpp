#include "../../include/runtime/goroutine.h"

#include <iostream>

namespace runtime {

    // TLS: 存储当前线程正在跑的协程指针
    static thread_local Goroutine* t_current_g = nullptr;
    // TLS: 记录切回点
    static thread_local ctx::fiber t_top_ctx;

    std::atomic<uint64_t> Goroutine::s_id_gen{1};

    Goroutine::Ptr Goroutine::current() {
        if (t_current_g) {
            return t_current_g->shared_from_this();
        }
        return nullptr;
    }

    Goroutine::Goroutine(Task task, size_t stack_size)
        : task_(std::move(task)) {
        id_ = s_id_gen.fetch_add(1);
        ctx_ = ctx::fiber(std::allocator_arg, ctx::fixedsize_stack(stack_size),
            [this](ctx::fiber&& sink) {
                t_top_ctx = std::move(sink);

                // 执行任务前，设置 TLS
                t_current_g = this;
                if (task_) task_();
                t_current_g = nullptr;

                finished_.store(true);
                return std::move(t_top_ctx);
            });
    }

    void Goroutine::resume() {
        if (!finished_ && ctx_) {
            // 进入协程前设置 TLS
            t_current_g = this;
            ctx_ = std::move(ctx_).resume();
            // 从协程出来后（可能是 yield 或结束），清除 TLS
            t_current_g = nullptr;
        }
    }

    void Goroutine::yield() {
        t_top_ctx = std::move(t_top_ctx).resume();
    }

} // namespace runtime