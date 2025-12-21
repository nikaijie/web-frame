#include <iostream>
#include "web/core/gee.h"
#include "runtime/scheduler.h"

int main() {
    runtime::Scheduler::get().start(8);

    // 2. 创建 Gee 引擎
    gee::Engine app;

    // 3. 注册业务路由 - 示例 1: 简单的 Ping-Pong
    app.GET("/ping", [](gee::WebContext *ctx) {
        ctx->res_.set_result(200, "success", "pong");
    });

    // 4. 注册业务路由 - 示例 2: 复杂的 JSON 数据 (利用 nlohmann/json)
    app.GET("/user/profile", [](gee::WebContext *ctx) {
        nlohmann::json data;
        data["id"] = 12345;
        data["name"] = "Gee_Developer";
        data["skills"] = {"C++", "Coroutine", "Network"};

        ctx->res_.set_result(200, "fetch profile success", data);
    });

    // 5. 注册业务路由 - 示例 3: 模拟耗时操作 (会自动触发协程挂起)
    app.GET("/async_data", [](gee::WebContext *ctx) {
        ctx->res_.set_result(200, "async result returned");
    });

    // 6. 启动引擎
    // Run 函数内部是一个主循环，不断 accept 新连接并开启 handle_http_task 协程
    std::cout << "Gee Framework is running on http://localhost:8080" << std::endl;
    app.Run(8080);

    return 0;
}
