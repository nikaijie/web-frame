#include "web/gee.h"
#include "web/request.h"
#include "web/response.h"
#include <iostream>
#include <map>

namespace web {

Gee::Gee() {
    std::cout << "Gee initialized" << std::endl;
}

Gee::~Gee() {
}

void Gee::GET(const std::string& path, HandlerFunc handler) {
    routes_[path] = handler;
    std::cout << "Registered GET route: " << path << std::endl;
}

void Gee::Run(const std::string& address) {
    std::cout << "Server would run on: " << address << std::endl;
    // 暂时不实现真正的web服务器
}

// 添加一个模拟请求的方法（用于测试）
void Gee::SimulateRequest(const std::string& method, const std::string& path) {
    auto it = routes_.find(path);
    if (it != routes_.end()) {
        Request req;
        req.path_ = path;
        req.method_ = method;
        
        Response resp;
        it->second(resp, req);
        
        std::cout << "Response: " << resp.ToString() << std::endl;
    } else {
        std::cout << "Route not found: " << path << std::endl;
    }
}

}
