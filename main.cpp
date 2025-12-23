#include <iostream>
#include"src/test/web.h"
#include "src/util/logger.h"

int main() {
    init_logging();
    runtime::Scheduler::get().start(8);
    db::init("127.0.0.1", "root", "123456789", "test", 50);
    gee::Engine app;
    app.GET("/ping", [](gee::WebContext *ctx) {
        auto results = db::table<Employee>("employees")
                .where("salary", ">", "8344")
                .model();

        ctx->JSON(gee::StateCode::OK, gee::statusToString(gee::Message::success), results);
    });

    app.GET("/user/:name", [](gee::WebContext *ctx) {
        auto name = ctx->Param("name");
        auto age = ctx->Param("age");

        // 手动构造一个 JSON 对象字符串
        std::string body;
        body.reserve(64);
        body.append("{\"name\":\"").append(name)
                .append("\",\"age\":").append(age).append("}");
        ctx->JSON(gee::StateCode::OK, "success", std::move(body));
    });

    app.GET("/user/info", [](gee::WebContext *ctx) {
        auto name = ctx->Query("name");
        std::string body = "\"" + name + "\"";
        ctx->JSON(gee::StateCode::OK, "success", std::move(body));
    });
    app.Run(8080);
    return 0;
}
