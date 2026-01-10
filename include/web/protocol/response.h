#pragma once
#include <string>

namespace gee {
    // --- 1. 业务状态码枚举 ---
    enum class StateCode {
        OK = 200,
        PARAM_ERROR = 400,
        AUTH_FAILED = 401,
        NOT_FOUND = 404,
        TIMEOUT = 408,
        SERVER_ERROR = 500
    };

    // --- 2. 业务消息枚举 ---
    enum class Message {
        success,
        failed,
        unknown
    };

    // --- 3. 枚举类转字符串工具 ---
    inline std::string statusToString(Message s) {
        switch (s) {
            case Message::success: return "success";
            case Message::failed: return "failed";
            case Message::unknown: return "unknown";
            default: return "invalid status";
        }
    }

    // --- 4. 响应实体类 ---
    struct Response {
        int state = 200; // 业务状态码
        std::string message; // 业务描述信息
        std::string body_buffer; // 存储业务 JSON 数据
        bool is_sent = false; // 状态锁：确保一个请求只发送一次

        // 填充业务数据逻辑
        void set_raw_data(int s, const std::string &m, std::string &&raw_json) {
            state = s;
            message = m;
            body_buffer = std::move(raw_json); // 移动语义，零拷贝
        }

        // 内部逻辑：将业务字段序列化为 JSON 字符串
        std::string serialize_json_body() const {
            std::string root;
            // 预分配内存：body内容 + message长度 + 基础JSON结构字符
            root.reserve(body_buffer.size() + message.size() + 64);

            root.append("{\"state\":").append(std::to_string(state));
            root.append(",\"message\":\"").append(message).append("\"");

            if (!body_buffer.empty()) {
                root.append(",\"data\":").append(body_buffer);
            }

            root.append("}");
            return root;
        }

        std::string build_http_packet(int http_code, const std::string &text = "") const {
            // 确定 Body 内容
            std::string content = text.empty() ? serialize_json_body() : text;

            // 构造完整的 HTTP 报文
            std::string packet;
            packet.reserve(content.size() + 128); // 预分配 Header 空间

            // 1. 状态行
            packet.append("HTTP/1.1 ").append(std::to_string(http_code)).append(" OK\r\n");

            // 2. 头部信息
            if (text.empty()) {
                packet.append("Content-Type: application/json; charset=utf-8\r\n");
            } else {
                packet.append("Content-Type: text/plain; charset=utf-8\r\n");
            }

            packet.append("Content-Length: ").append(std::to_string(content.size())).append("\r\n");
            packet.append("Connection: close\r\n");
            packet.append("\r\n"); // Header 和 Body 的分界线

            // 3. 报文体
            packet.append(content);

            return packet;
        }
    };
}
