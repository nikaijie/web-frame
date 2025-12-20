#include "../../include/runtime/goroutine.h"
#include <iostream>

namespace runtime {

    // TLS (线程局部存储)：记录当前物理线程正在跑的协程“切回点”
    static thread_local ctx::fiber t_top_ctx;

    std::atomic<uint64_t> Goroutine::s_id_gen{1};

    Goroutine::Goroutine(Task task, size_t stack_size)
        : task_(std::move(task)) {
        id_ = s_id_gen.fetch_add(1);

        // 创建上下文
        ctx_ = ctx::fiber(std::allocator_arg, ctx::fixedsize_stack(stack_size),
            [this](ctx::fiber&& sink) {
                // sink 代表了调用 resume() 的那个位置（即调度器/Main）
                t_top_ctx = std::move(sink);

                if (task_) {
                    task_();
                }

                finished_.store(true);

                // 协程结束，必须返回一个有效的上下文切回去
                return std::move(t_top_ctx);
            });
    }

    void Goroutine::resume() {
        if (!finished_ && ctx_) {
            // 切换：执行流跳进 ctx_，同时返回的值就是我们跳出来的地方
            ctx_ = std::move(ctx_).resume();
        }
    }

    void Goroutine::yield() {
        // 切出：跳回到 t_top_ctx 记录的位置
        t_top_ctx = std::move(t_top_ctx).resume();
    }

} // namespace runtime