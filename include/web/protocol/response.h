#pragma once
#include <string>

namespace gee {
    struct Response {
        int state = 200;
        std::string message;
        std::string body_buffer; // 存储序列化好的 data 内容
        bool has_custom_state = false;

        // 极致性能：直接通过 move 接收已经在外部拼接好的字符串
        void set_raw_data(int s, const std::string &m, std::string &&raw_json) {
            state = s;
            message = m;
            body_buffer = std::move(raw_json); // 仅仅是指针交换，0 拷贝
            has_custom_state = true;
        }

        // 序列化整个报文外壳
        std::string serialize() const {
            // 预估外壳长度：state(10) + message + data + 符号(50)
            std::string root;
            root.reserve(body_buffer.size() + message.size() + 64);

            root.append("{\"state\":").append(std::to_string(state));
            root.append(",\"message\":\"").append(message).append("\"");

            if (!body_buffer.empty()) {
                root.append(",\"data\":").append(body_buffer);
            }

            root.append("}");
            return root;
        }
    };
}
