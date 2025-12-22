#include <iostream>
#include "web/core/gee.h"
#include "runtime/scheduler.h"
#include "../src/db/db.h"
#include "../src/db/orm/model.h"
#include "../src/db/orm/query_builder.h"

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

    void write_json(std::string &out) const {
        out.append("{\"id\":").append(std::to_string(id))
                .append(",\"name\":\"").append(name)
                .append("\",\"department\":\"").append(department)
                .append("\",\"salary\":").append(std::to_string(salary))
                .append("}");
    }

    // 静态函数：序列化 Employee 数组为 JSON
    static std::string serialize_array(const std::vector<Employee> &employees) {
        std::string json;
        json.reserve(employees.size() * 120 + 2);
        json.push_back('[');

        for (size_t i = 0; i < employees.size(); ++i) {
            employees[i].write_json(json);
            if (i + 1 < employees.size()) json.push_back(',');
        }

        json.push_back(']');
        return json; // NRVO，返回时 move
    }
};


int web_text() {
    runtime::Scheduler::get().start(8);
    db::init("127.0.0.1", "root", "123456789", "test", 50);
    // 2. 创建 Gee 引擎
    gee::Engine app;

    // 3. 注册业务路由 - 示例 1: 简单的 Ping-Pong
    app.GET("/ping", [](gee::WebContext *ctx) {
        // 1. 从数据库取数据
        auto results = db::table<Employee>("employees")
                .where("salary", ">", "8344")
                .model();

        std::string json = Employee::serialize_array(results);

        // 4. 交给 Response
        ctx->res_.set_raw_data(200, "success", std::move(json));

        // 5. 触发发送
        ctx->String(200, "");
    });

    // 6. 启动引擎
    // Run 函数内部是一个主循环，不断 accept 新连接并开启 handle_http_task 协程
    std::cout << "Gee Framework is running on http://localhost:8080" << std::endl;
    app.Run(8080);

    return 0;
}
