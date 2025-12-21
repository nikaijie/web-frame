#pragma once
#include <boost/context/fiber.hpp>
#include <atomic>
#include <functional>
#include <memory>

namespace runtime {
    namespace ctx = boost::context;

    // 继承 enable_shared_from_this 使得我们可以安全地从类内部获取 shared_ptr
    class Goroutine : public std::enable_shared_from_this<Goroutine> {
    public:
        using Ptr = std::shared_ptr<Goroutine>;
        using Task = std::function<void()>;

        Goroutine(Task task, size_t stack_size = 64 * 1024);

        void resume();

        static void yield();

        // TLS 机制：获取当前线程正在执行的协程
        static Goroutine::Ptr current();

        bool is_finished() const { return finished_.load(); }
        uint64_t id() const { return id_; }

    private:
        uint64_t id_;
        ctx::fiber ctx_;
        std::atomic<bool> finished_{false};
        Task task_;

        static std::atomic<uint64_t> s_id_gen;
    };

    void go(Goroutine::Task task);
} // namespace runtime
