#include <iostream>
#include <vector>
#include <atomic>
#include <chrono>
#include <sys/resource.h>
#include <unistd.h>

#include "../src/db/db.h"
#include "runtime/scheduler.h"
#include "runtime/netpoller.h"

// --- 内存监控工具函数 ---
void print_mem_usage(const std::string &tag) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);

    double mb = 0;
#ifdef __APPLE__
    // macOS: 单位是字节 (Bytes)
    mb = usage.ru_maxrss / 1024.0 / 1024.0;
#else
    // Linux: 单位是千字节 (KB)
    mb = usage.ru_maxrss / 1024.0;
#endif

    std::cout << "\n>>>> " << tag << " <<<<" << std::endl;
    std::cout << "物理内存峰值占用 (RSS): " << mb << " MB" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
}

// --- 实体类定义 (极致性能版) ---
struct Employee : public db::Model {
    int id;
    std::string name;
    std::string department;
    int salary;

    std::string table_name() const override {
        return "employees";
    }

    // 性能核心：通过 char** 原始指针直接赋值，避免 Map 开销
    void from_row(db::RawRow row) override {
        // row[i] 直接指向驱动内部缓冲区
        id = row[0] ? std::atoi(row[0]) : 0;
        name = row[1] ? row[1] : "";
        department = row[2] ? row[2] : "";
        salary = row[3] ? std::atoi(row[3]) : 0;
    }

    // 用于写入操作的映射
    db::FieldMap to_row() const override {
        return {
            {"id", std::to_string(id)},
            {"name", name},
            {"department", department},
            {"salary", std::to_string(salary)}
        };
    }
};

int mysql_test() {
    // 1. 记录初始内存
    print_mem_usage("程序启动（初始状态）");

    // 2. 环境初始化
    db::init("127.0.0.1", "root", "123456789", "test", 50);
    runtime::Scheduler::get().start(8);

    int total_tasks = 5000;
    std::atomic<int> finished_count{0};

    std::cout << "开始 5000 个高并发 SQL 查询测试..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    // 3. 批量启动协程
    for (int i = 0; i < total_tasks; ++i) {
        runtime::go([&finished_count, i]() {
            auto results = db::table<Employee>("employees")
                    .where("salary", ">", "5000")
                    .model();

            // 采样打印，避免控制台 IO 成为瓶颈
            if (i % 500 == 0) {
                std::cout << "[协程 " << i << "] 查询成功，返回行数: " << results.size() << std::endl;
            }


            finished_count++;
        });
    }

    // 4. 主线程等待所有协程结束
    while (finished_count < total_tasks) {
        std::this_thread::yield();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // 5. 记录峰值内存
    print_mem_usage("测试完成（内存峰值）");

    std::cout << "\n========================================" << std::endl;
    std::cout << "C++ 协程 ORM 测试报告" << std::endl;
    std::cout << "并发协程数: " << total_tasks << std::endl;
    std::cout << "总耗时: " << ms << " ms" << std::endl;
    std::cout << "平均 TPS: " << (total_tasks * 1000.0 / ms) << " req/s" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
