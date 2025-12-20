#pragma once

#include <string>
#include <functional>
#include <map>

namespace web {
    class Request;
    class Response;

    // 处理函数类型（Web层接口）
    using HandlerFunc = std::function<void(Response&, const Request&)>;

    class Gee {
    private:
        std::map<std::string, HandlerFunc> routes_;

    public:
        Gee();
        ~Gee();

        // 注册路由
        void GET(const std::string& path, HandlerFunc handler);

        // 启动服务（内部会调用运行时初始化）
        void Run(const std::string& address);
        
        // 模拟请求（用于测试，不需要真正的web服务器）
        void SimulateRequest(const std::string& method, const std::string& path);
    };
}

