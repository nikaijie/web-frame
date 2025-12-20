#pragma once
#include <boost/context/fiber.hpp>
#include <atomic>
#include <functional>
#include <memory>

namespace runtime {

    namespace ctx = boost::context;

    class Goroutine {
    public:
        using Ptr = std::shared_ptr<Goroutine>;
        using Task = std::function<void()>;

        // 构造函数：接受业务逻辑
        Goroutine(Task task, size_t stack_size = 64 * 1024);

        // 恢复执行
        void resume();

        // 静态方法：让当前正在运行的协程主动让出
        static void yield();

        bool is_finished() const { return finished_.load(); }
        uint64_t id() const { return id_; }

    private:
        uint64_t id_;
        ctx::fiber ctx_;                // 协程上下文
        std::atomic<bool> finished_{false};
        Task task_;

        static std::atomic<uint64_t> s_id_gen;
    };

    // 模拟 go 关键字
    void go(Goroutine::Task task);

} // namespace runtime