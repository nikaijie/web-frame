#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

// 建议不要在头文件全局使用 using namespace，容易引发冲突
using json = nlohmann::json;

namespace gee {
    struct Response {
        int state = 200;
        std::string message;

        // --- 修正 1: 将类型改为 json ---
        json data = nullptr;

        bool has_custom_state = false;

        // --- 修正 2: 保持 json 对象的传递 ---
        void set_result(int s, const std::string &m, json d = nullptr) {
            state = s;
            message = m;
            data = std::move(d); // 现在是 json 到 json 的移动，效率很高
            has_custom_state = true;
        }

        // 核心序列化逻辑
        std::string serialize() const {
            json root;
            root["state"] = state;
            root["message"] = message;

            // --- 修正 3: 现在 data 是 json 对象，可以调用 is_null() ---
            if (!data.is_null()) {
                root["data"] = data;
            }
            return root.dump();
        }
    };
}
