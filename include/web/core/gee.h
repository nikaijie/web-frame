#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "node.h"
#include "runtime/context/web_context.h"

namespace gee {
    using HandlerFunc = std::function<void(WebContext *)>;
    class Engine; // 前置声明

    // --- RouterGroup 声明 ---
    class RouterGroup {
    public:
        RouterGroup(std::string prefix, Engine *engine)
            : prefix_(std::move(prefix)), engine_(engine) {
        }

        RouterGroup *Group(std::string suffix); // 实现在下面或 cpp 中

        void Use(HandlerFunc middleware) {
            middlewares_.push_back(std::move(middleware));
        }

        // 这里只声明，不要写调用 engine_ 的代码
        void GET(std::string path, HandlerFunc handler);

        void POST(std::string path, HandlerFunc handler);

    protected: // 改为 protected 方便 Engine 访问
        std::string prefix_;
        std::vector<HandlerFunc> middlewares_;
        Engine *engine_;
    };

    // --- Engine 声明 ---
    class Engine : public RouterGroup {
    public:
        Engine(); // 构造函数实现在下面
        ~Engine() = default;

        // 禁止拷贝
        Engine(const Engine &) = delete;

        Engine &operator=(const Engine &) = delete;

        // 核心方法
        void handle_http_task(int fd);

        void Run(int port);

        std::vector<std::string> parse_pattern(std::string_view pattern);

        // 关键：修改 add_route 签名，使其能接收中间件链
        void add_route(std::string method, std::string path, HandlerFunc handler,
                       std::vector<HandlerFunc> middlewares = {});

        std::pair<Node *, std::unordered_map<std::string, std::string> >
        get_route(const std::string &method, const std::string &path);

    private:
        std::unordered_map<std::string, Node *> roots_;
        // 存储 完整执行链：Key="GET-/user/:id"
        std::unordered_map<std::string, std::vector<HandlerFunc> > route_handlers_chain_;

        int create_listen_socket(int port);
    };

    // --- 重点：在两个类都定义完后，再写相互调用的函数实现 ---

    inline Engine::Engine() : RouterGroup("", this) {
    }

    inline RouterGroup *RouterGroup::Group(std::string suffix) {
        return new RouterGroup(prefix_ + suffix, engine_);
    }

    inline void RouterGroup::GET(std::string path, HandlerFunc handler) {
        // 此时 Engine 类已经定义完整，编译器知道 add_route 是什么了
        engine_->add_route("GET", prefix_ + path, std::move(handler), middlewares_);
    }

    inline void RouterGroup::POST(std::string path, HandlerFunc handler) {
        engine_->add_route("POST", prefix_ + path, std::move(handler), middlewares_);
    }
} // namespace gee
