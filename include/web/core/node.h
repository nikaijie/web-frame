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

        // 查找子节点
        Node *child = match_child(part);

        // 如果没找到且不是模糊匹配，则新建节点
        // 注意：这里要处理 child->part == part 的判断，避免重复创建
        // 优化：match_child 在查找插入位置时应该找完全匹配的 part
        if (child == nullptr || child->part != part) {
            child = new Node();
            child->part = part;
            child->is_wild = (part[0] == ':' || part[0] == '*');
            children.push_back(child);
        }

        // 递归向下
        child->insert(pattern, parts, height + 1);
    }

    Node *search(const std::vector<std::string> &parts, int height) {
        // 递归终止条件：搜到了末尾，或者遇到了通配符 *
        if (parts.size() == height || (part.size() > 0 && part[0] == '*')) {
            if (pattern.empty()) {
                return nullptr; // 虽然路径对，但这个节点没注册过 handler（不是叶子节点）
            }
            return this;
        }

        std::string part = parts[height];
        // 拿到所有可能的子节点（静态匹配 + 动态匹配）
        std::vector<Node *> list = match_children(part);

        for (Node *child: list) {
            Node *result = child->search(parts, height + 1);
            if (result != nullptr) return result;
        }

        return nullptr;
    }
};
