#include <iostream>
#include <spdlog/spdlog.h>

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
};


int web_text() {
    runtime::Scheduler::get().start(8);
    db::init("127.0.0.1", "root", "123456789", "test", 50);
    // 2. 创建 Gee 引擎
    gee::Engine app;

    app.GET("/ping", [](gee::WebContext *ctx) {
        auto results = db::table<Employee>("employees")
                .where("salary", ">", "8344")
                .model();

        ctx->JSON(gee::StateCode::OK, gee::statusToString(gee::Message::success), results);
    });

    app.GET("/user/:name/:age", [](gee::WebContext *ctx) {
        auto name = ctx->Param("name");
        auto age = ctx->Param("age");

        // 手动构造一个 JSON 对象字符串
        std::string body;
        body.reserve(64);
        body.append("{\"name\":\"").append(name)
                .append("\",\"age\":").append(age).append("}");
        ctx->JSON(gee::StateCode::OK, "success", std::move(body));
    });

    app.GET("/user", [](gee::WebContext *ctx) {
        auto name = ctx->Query("name");
        std::string body = "\"" + name + "\"";
        ctx->JSON(gee::StateCode::OK, "success", std::move(body));
    });
    spdlog::info("Gee Framework is running on http://localhost:8080");
    app.Run(8080);

    return 0;
}
