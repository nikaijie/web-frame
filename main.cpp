#include <iostream>

#include "data_structure/channel.h"
#include "data_structure/context.h"
#include "data_structure/wait_group.h"
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

auto TimeoutMiddleware(int timeout_ms) {
    return [timeout_ms](gee::WebContext *c) {

        auto ctx = runtime::Context::WithTimeout(timeout_ms);
        auto biz_done = std::make_shared<runtime::Channel<bool>>(1);

        //业务代码
        runtime::go([c, biz_done]() {
            c->Next();
            biz_done->push(true);
        });


        auto wait_winner = std::make_shared<runtime::Channel<int>>(1);

        runtime::go([biz_done, wait_winner]() {
            biz_done->pop();
            wait_winner->push(1); // 業務勝出
        });

        runtime::go([ctx, wait_winner]() {
            ctx->done()->pop();   // 這裡會被 Context 內部的 Timer 喚醒
            wait_winner->push(2); // 超時勝出
        });

        // 阻塞等待賽跑結果
        int winner = wait_winner->pop();

        if (winner == 2) {
            if (!c->is_aborted()) { // 防止重複寫入
                c->JSON(gee::StateCode::TIMEOUT, "Request Timeout",
                        "{}");
                c->Abort(); // 標記 Abort，阻止後續邏輯影響響應
            }
        }
    };
}

int main() {
    init_logging();
    runtime::Scheduler::get().start(8);
    db::init("127.0.0.1", "root", "123456789", "test", 50);
    gee::Engine app;

    app.Use(LoggerMiddleware);
    app.Use(TimeoutMiddleware(3000));

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

    app.GET("/user/print_test", [](gee::WebContext *ctx) {
        auto wg = std::make_shared<runtime::WaitGroup>();
        auto chan1 = std::make_shared<runtime::Channel<int> >(0);
        auto chan2 = std::make_shared<runtime::Channel<int> >(0);
        printf("[Main] Start\n");
        wg->add(2);
        runtime::go([chan1, chan2,wg]() {
            for (int i = 1; i <= 10; i += 2) {
                // 第一次直接跑，后续等 chan1 的信号
                if (i > 1) chan1->pop();
                printf("Coroutine A: %d\n", i);
                chan2->push(1);
            }
            wg->done(); // 完成
        });
        runtime::go([chan1, chan2,wg]() {
            for (int i = 2; i <= 10; i += 2) {
                // 等待协程 A 的信号
                chan2->pop();

                printf("Coroutine B: %d\n", i);

                // 打印完通知协程 A
                chan1->push(1);
            }
            wg->done();
        });
        printf("[Main] Waiting...\n");
        wg->wait();
        printf("[Main] Woke up!\n");
        ctx->JSON(gee::StateCode::OK, gee::statusToString(gee::Message::success),
                  "{}");
        printf("[Main] After JSON\n");
    });
    app.Run(8080);
    return 0;
}
