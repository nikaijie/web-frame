#pragma once


#include <string>
#include <vector>
#include <unordered_map>
#include <functional>


struct Node {
    std::string pattern; // 待匹配路由，例如 /user/:id
    std::string part; // 路由中的一部分，例如 :id
    std::vector<Node *> children;
    bool is_wild; // 是否包含 : 或 *

    // 析构函数记得释放子节点，防止内存泄漏
    ~Node() {
        for (auto child: children) delete child;
    }

    // 辅助方法：寻找第一个匹配成功的子节点（用于插入）
    Node *match_child(const std::string &part) {
        for (auto child: children) {
            if (child->part == part || child->is_wild) return child;
        }
        return nullptr;
    }

    // 辅助方法：寻找所有匹配成功的子节点（用于搜索）
    std::vector<Node *> match_children(const std::string &part) {
        std::vector<Node *> nodes;
        for (auto child: children) {
            if (child->part == part || child->is_wild) {
                nodes.push_back(child);
            }
        }
        return nodes;
    }

    void insert(const std::string &pattern, const std::vector<std::string> &parts, int height) {
        if (parts.size() == height) {
            this->pattern = pattern;
            return;
        }

        std::string part = parts[height];

        // 插入时必须：寻找 part 字符串完全一致的子节点
        // 不能使用原来的 match_child，因为插入是为了“建树”，必须精确
        Node *child = nullptr;
        for (auto c: children) {
            if (c->part == part) {
                child = c;
                break;
            }
        }

        // 如果没找到完全匹配的，才新建
        if (child == nullptr) {
            child = new Node();
            child->part = part;
            child->is_wild = (part[0] == ':' || part[0] == '*');
            children.push_back(child);
        }

        child->insert(pattern, parts, height + 1);
    }

    // 传入 params 引用，用于存储 :id -> "123" 的映射
    Node *search(const std::vector<std::string> &parts, int height,
                 std::unordered_map<std::string, std::string> &params) {
        //  递归终止：搜到末尾或遇到 *
        if (parts.size() == height || (this->part.size() > 0 && this->part[0] == '*')) {
            if (this->pattern.empty()) return nullptr;
            return this;
        }

        std::string target_part = parts[height];

        // --- 优先级 1：精确匹配 ---
        for (Node *child: children) {
            if (!child->is_wild && child->part == target_part) {
                Node *result = child->search(parts, height + 1, params);
                if (result) return result;
            }
        }

        // --- 优先级 2：动态参数 (:) ---
        for (Node *child: children) {
            if (child->part.size() > 0 && child->part[0] == ':') {
                Node *result = child->search(parts, height + 1, params);
                if (result) {
                    // 在这里记录参数！
                    // key 是 ":id" 去掉冒号，value 是当前路径里的字符串
                    params[child->part.substr(1)] = target_part;
                    return result;
                }
            }
        }

        // --- 优先级 3：通配符 (*) ---
        for (Node *child: children) {
            if (child->part.size() > 0 && child->part[0] == '*') {
                // 通配符匹配剩下的所有部分，通常存为 path
                // 这里逻辑简单处理，直接返回
                return child;
            }
        }

        return nullptr;
    }
};
