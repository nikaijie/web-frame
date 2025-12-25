#pragma once
#include <string>
#include <unordered_map>
#include <functional>


#include "node.h"
#include "runtime/context/web_context.h"


namespace gee {
    // 定义 Handler 函数原型，接收 Context 指针
    using HandlerFunc = std::function<void(WebContext *)>;

    class Engine {
    public:
        Engine() = default;

        ~Engine() = default;

        // 禁止拷贝
        Engine(const Engine &) = delete;

        Engine &operator=(const Engine &) = delete;

        // 注册路由接口
        void GET(std::string path, HandlerFunc handler) {
            add_route("GET", path, std::move(handler));
        }

        void POST(std::string path, HandlerFunc handler) {
            add_route("POST", path, std::move(handler));
        }

        void Use(HandlerFunc middleware) {
            middlewares_.push_back(std::move(middleware));
        }

        // 核心：主线程将读取到的 fd 和原始数据交给框架
        void handle_http_task(int fd);

        void Run(int port);

        std::vector<std::string> parse_pattern(std::string_view pattern);

        void add_route(std::string method, std::string path, HandlerFunc handler);

        std::pair<Node *, std::unordered_map<std::string, std::string> >
        get_route(const std::string &method, const std::string &path);

    private:
        // 路由表：Key 格式为 "METHOD-PATH"
        std::unordered_map<std::string, HandlerFunc> routes_;
        std::unordered_map<std::string, Node *> roots_;

        std::vector<HandlerFunc> middlewares_;

        int create_listen_socket(int port);
    };
} // namespace gee
