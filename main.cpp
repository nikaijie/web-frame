#include "runtime/scheduler.h"
#include <chrono>
#include "src/db/MySQlPool.cpp"

#include <atomic>

int main() {
    // 1. 初始化连接池（此时 Worker 线程还没开始高频切换）
    // 假设我们预连 50 个，支撑 1000 个任务
    MySQLPool::get().init("127.0.0.1", "root", "123456789", "test", 50);

    // 2. 启动调度器和 IO 线程
    runtime::Scheduler::get().start(8);
    std::thread io_thread([]() { runtime::Netpoller::get().poll_loop(); });
    io_thread.detach();

    db::MySQLCoro client;
    int total = 100;
    int tmp = total;
    std::atomic<int> cnt{total};
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total; ++i) {
        runtime::go([&client, &cnt, i]() {
            // 从池子拿连接
            MYSQL *conn = nullptr;
            while (!(conn = MySQLPool::get().acquire())) {
                // 如果池子空了，当前协程让出 CPU，等下一轮轮转
                runtime::Goroutine::yield();
            }

            // 执行查询（异步，会 yield）
            client.query(conn, "SELECT * from employees;");

            // 归还连接
            MySQLPool::get().release(conn);

            cnt--;
        });
    }

    // 等待所有协程完成
    while (cnt > 0) { std::this_thread::yield(); }

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << tmp << "个任务耗时: " << ms << "ms" << std::endl;

    return 0;
}
