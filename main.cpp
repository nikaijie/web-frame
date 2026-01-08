#include <iostream>
#include"src/test/web.h"
#include "src/util/logger.h"


void LoggerMiddleware(gee::WebContext *c) {
    auto start = std::chrono::high_resolution_clock::now();

    spdlog::info("--> [Middleware] Start: {}  {}", c->method(), c->path());

    c->Next();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    spdlog::info("<-- [Middleware] Done: {}  {} us", c->path(), duration);
}

void AuthMiddleware(gee::WebContext *c) {
    std::string token = c->Query("token");
    if (token != "zhaixing") {
        spdlog::info("[Auth] Token verified.");
        c->Next();
    } else {
        spdlog::warn("[Auth] Unauthorized access to {}", c->path());
        c->JSON(gee::StateCode::PARAM_ERROR, "Invalid Token", "{\"error\": \"Unauthenticated\"}");
        c->Abort(); // 拦截后续业务
    }
}

int main() {
    init_logging();
    runtime::Scheduler::get().start(8);
    db::init("127.0.0.1", "root", "123456789", "test", 50);
    gee::Engine app;

    app.Use(LoggerMiddleware);

    auto api_group = app.Group("/api");
    api_group->Use(AuthMiddleware);
    app.GET("/getUser", [](gee::WebContext *ctx) {
        auto results = db::table<Employee>("employees")
                .where("salary", ">", "8344")
                .model();

        ctx->JSON(gee::StateCode::OK, gee::statusToString(gee::Message::success), results);
    });

    api_group->GET("/user/:name", [](gee::WebContext *ctx) {
        auto name = ctx->Param("name");
        auto age = ctx->Param("age");

        //   手动构造一个 JSON 对象字符串
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

    app.POST("/login", [](gee::WebContext *c) {
        // 从我们刚写好的 post_form_ 中取值
        std::string username = c->PostForm("username");
        std::string password = c->PostForm("password");
        std::string body;
        body.reserve(64);
        body.append("{\"username\":\"").append(username)
                .append("\",\"password\":").append(password).append("}");
        if (username == "zhaixing" && password == "123") {
            c->JSON(gee::StateCode::OK, gee::statusToString(gee::Message::success), std::move(body));
        } else {
            c->JSON(gee::StateCode::OK, gee::statusToString(gee::Message::failed), "{}");
        }
    });
    app.Run(8080);
    return 0;
}
