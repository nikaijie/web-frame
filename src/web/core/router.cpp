#include <spdlog/spdlog.h>

#include "web/core/gee.h"


namespace gee {

    void Engine::add_route(std::string method, std::string path, HandlerFunc handler,
                           std::vector<HandlerFunc> group_middlewares) {
        std::vector<std::string> parts = parse_pattern(path);

        if (roots_.find(method) == roots_.end()) {
            roots_[method] = new Node();
        }
        roots_[method]->insert(path, parts, 0);

        std::vector<HandlerFunc> chain;

        chain.insert(chain.end(), group_middlewares.begin(), group_middlewares.end());
        chain.push_back(std::move(handler));

        std::string key = method + "-" + path;
        route_handlers_chain_[key] = std::move(chain);
        spdlog::info("Route registered: {} - {}", method, path);
    }


    std::vector<std::string> Engine::parse_pattern(std::string_view pattern) {
        std::vector<std::string> parts;
        std::string current;

        for (char c: pattern) {
            if (c == '/') {
                if (!current.empty()) {
                    parts.push_back(current);
                    // 核心逻辑：如果是通配符，直接跳出循环
                    if (current[0] == '*') return parts;
                    current.clear();
                }
            } else {
                current += c;
            }
        }

        // 处理最后一个片段
        if (!current.empty()) {
            parts.push_back(current);
        }

        return parts;
    }

    std::pair<Node *, std::unordered_map<std::string, std::string> >
    Engine::get_route(const std::string &method, const std::string &path) {
        // 1. 解析请求路径
        std::vector<std::string> searchParts = parse_pattern(path);
        std::unordered_map<std::string, std::string> params;

        // 2. 检查对应方法的根节点是否存在
        auto it = roots_.find(method);
        if (it == roots_.end()) {
            return {nullptr, {}};
        }

        Node *root = it->second;

        Node *n = root->search(searchParts, 0, params);

        if (n != nullptr) {
            // 4. 【大幅简化】：参数已经在 search 过程中填好了，直接返回
            return {n, params};
        }

        return {nullptr, {}};
    }
}
