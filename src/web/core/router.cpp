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
        // 1. 解析请求路径 (例如: "/user/zhaixing/25" -> ["user", "zhaixing", "25"])
        std::vector<std::string> searchParts = parse_pattern(path);
        std::unordered_map<std::string, std::string> params;

        // 2. 检查对应方法的根节点是否存在
        auto it = roots_.find(method);
        if (it == roots_.end()) {
            return {nullptr, {}};
        }

        Node *root = it->second;
        // 3. 调用 Trie 树的搜索逻辑
        Node *n = root->search(searchParts, 0);

        if (n != nullptr) {
            // 4. 匹配成功，开始提取参数
            // 解析注册时的规则 (例如: "/user/:name/:age" -> ["user", ":name", ":age"])
            std::vector<std::string> parts = parse_pattern(n->pattern);

            for (size_t i = 0; i < parts.size(); ++i) {
                // 处理 :name 类型的动态参数
                if (parts[i][0] == ':') {
                    params[parts[i].substr(1)] = searchParts[i];
                }
                // 处理 *filepath 类型的通配符参数
                if (parts[i][0] == '*' && parts[i].size() > 1) {
                    // 将 searchParts 中从当前索引到末尾的所有部分用 "/" 连接
                    std::string joinedPath;
                    for (size_t j = i; j < searchParts.size(); ++j) {
                        if (j > i) joinedPath += "/";
                        joinedPath += searchParts[j];
                    }
                    params[parts[i].substr(1)] = joinedPath;
                    break;
                }
            }
            return {n, params};
        }

        return {nullptr, {}};
    }
}
