#include <spdlog/spdlog.h>

#include "web/core/gee.h"


namespace gee {
    void Engine::add_route(std::string method, std::string pattern, HandlerFunc handler) {
        // 1. 解析路径为 parts (例如: "/user/:id" -> ["user", ":id"])
        std::vector<std::string> parts = parse_pattern(pattern);


        std::string key = method + "-" + pattern;

        // 3. 检查并初始化该方法的 Trie 树根节点
        if (roots_.find(method) == roots_.end()) {
            roots_[method] = new Node(); // 这里的 Node 是之前定义的 Trie 节点
        }

        // 4. 将路径插入 Trie 树
        // insert 会在树中创建相应的节点路径，并把 pattern 存入叶子节点
        roots_[method]->insert(pattern, parts, 0);

        // 5. 存储 Handler 回调
        routes_[key] = std::move(handler);

        spdlog::info("Route registered: {} - {}", method, pattern);
    }


    std::vector<std::string> Engine::parse_pattern(std::string_view pattern) {
        std::vector<std::string> parts;
        std::string current;

        // 模拟 Go 的 strings.Split 行为
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

        // 3. 【关键修改】：调用搜索逻辑时，直接把 params 传进去
        // Node 内部会根据优先级匹配，并自动填好 :id 或 * 通配符对应的参数
        Node *n = root->search(searchParts, 0, params);

        if (n != nullptr) {
            // 4. 【大幅简化】：参数已经在 search 过程中填好了，直接返回
            return {n, params};
        }

        return {nullptr, {}};
    }
}
