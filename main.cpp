#include "runtime/scheduler.h"
#include <iostream>
#include <chrono>

using namespace runtime;

int main() {
    // 1. 启动调度器（开启 4 个 Worker 线程）
    Scheduler::get().start(4);

    // 2. 使用 go 启动多个协程
    for (int i = 0; i < 5; ++i) {
        go([i]() {
            std::cout << "Goroutine " << i << " 正在执行第一阶段" << std::endl;

            Goroutine::yield(); // 模拟阻塞，让出 CPU

            std::cout << "Goroutine " << i << " 正在执行第二阶段" << std::endl;
        });
    }

    // 防止主线程退出
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return 0;
}