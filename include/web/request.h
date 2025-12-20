#pragma once
#include <string>
#include <unordered_map>

namespace web {
    class Request {
    private:
        std::string method_;  // GET/POST等
        std::string path_;    // 请求路径
        std::unordered_map<std::string, std::string> headers_;
        std::string body_;

    public:
        const std::string& Path() const { return path_; }
        const std::string& Method() const { return method_; }
        // 其他getter/setter（仅处理数据，不涉及IO）
        
        // 允许Gee类访问私有成员来设置路径和方法
        friend class Gee;
    };
}

