#include "runtime/scheduler.h"
#include <chrono>

#include "src/db/db.h"
#include <iostream>
#include <vector>
#include <atomic>

#include "runtime/netpoller.h"

// 继承 db::Model 实现映射
struct Employee : public db::Model {
    int id;
    std::string name;
    std::string department;
    int salary;

    // 告诉框架表名
    std::string table_name() const override { return "employees"; }

    // 从数据库 Row 映射到 C++ 对象
    void from_row(const db::RowData &data) override {
        id = std::stoi(data.at("id"));
        name = data.at("name");
        department = data.at("department");
        salary = std::stoi(data.at("salary"));
    }

    // 从 C++ 对象映射回 Row (用于 Insert/Update)
    db::RowData to_row() const override {
        return {
            {"id", std::to_string(id)},
            {"name", name},
            {"department", department},
            {"salary", std::to_string(salary)}
        };
    }
};


int main() {
    db::init("127.0.0.1", "root", "123456789", "test", 50);

    runtime::Scheduler::get().start(8);


    int total_tasks = 500;
    std::atomic<int> finished_count{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    // 4. 批量启动协程
    for (int i = 0; i < total_tasks; ++i) {
        runtime::go([&finished_count, i]() {
            // 使用链式调用查询：SELECT * FROM employees WHERE salary > '5000';
            auto results = db::table<Employee>("employees")
                    .where("salary", ">", "5000")
                    .model();

            // 每完成 100 个打印一次进度
            if (i % 100 == 0) {
                std::cout << "[协程 " << i << "] 查询成功，匹配员工数: " << results.size() << std::endl;
            }

            finished_count++;
        });
    }

    // 5. 主线程等待所有协程结束
    while (finished_count < total_tasks) {
        std::this_thread::yield();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    std::cout << "\n========================================" << std::endl;
    std::cout << "测试完成！" << std::endl;
    std::cout << "并发协程数: " << total_tasks << std::endl;
    std::cout << "连接池大小: 50" << std::endl;
    std::cout << "总耗时: " << ms << " ms" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
