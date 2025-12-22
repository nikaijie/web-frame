#include "runtime/context/web_context.h"

#include <charconv>

#include "runtime/web_io.h"


#include <string_view>

namespace gee {
    bool WebContext::parse() {
        // --- 阶段 1: 读取并解析 Header ---
        while (true) {
            size_t header_end_pos = find_header_end();
            if (header_end_pos != std::string_view::npos) {
                // 找到了 \r\n\r\n，开始解析 Header
                if (!do_parse_header(header_end_pos + 4)) return false;
                break;
            }

            if (runtime::web_read(this->fd, this->raw_data_) <= 0) return false;
        }

        // --- 阶段 2: 根据 Content-Length 读取 Body ---
        if (req_.content_length > 0) {
            while (raw_data_.size() < req_.header_size + req_.content_length) {
                if (runtime::web_read(this->fd, this->raw_data_) <= 0) return false;
            }
            // 绑定 Body 的 string_view
            req_.body = std::string_view(raw_data_.data() + req_.header_size, req_.content_length);
        }

        return true;
    }

    void WebContext::String(int code, const std::string &text) {
        std::string final_body;

        // 如果业务层手动传了 text (比如返回 HTML 或自定义文本)
        if (!text.empty()) {
            final_body = text;
        } else {
            // 否则，自动序列化 Response 对象里的 state, message, data
            final_body = res_.serialize();
        }

        // 构造完整的 HTTP 响应报文
        std::string http_packet;
        http_packet.reserve(256 + final_body.size());

        // 注意：状态码优先使用 res_ 里的业务状态码
        int status = (res_.state != 200) ? res_.state : code;

        http_packet += "HTTP/1.1 " + std::to_string(status) + " OK\r\n";
        http_packet += "Content-Type: application/json; charset=utf-8\r\n";
        http_packet += "Content-Length: " + std::to_string(final_body.size()) + "\r\n";
        http_packet += "Connection: close\r\n";
        http_packet += "\r\n";
        http_packet += final_body;

        // 协程版异步写入，发送到网络
        runtime::web_write(this->fd, http_packet.data(), http_packet.size());
    }

    // 辅助：查找 \r\n\r\n
    size_t WebContext::find_header_end() {
        std::string_view sv(raw_data_.data(), raw_data_.size());
        return sv.find("\r\n\r\n");
    }

    bool WebContext::do_parse_header(size_t total_header_size) {
        req_.header_size = total_header_size;
        std::string_view full_data(raw_data_.data(), total_header_size);

        // 1. 解析请求行 (e.g., "POST /api/data HTTP/1.1")
        size_t line_end = full_data.find("\r\n");
        if (line_end == std::string_view::npos) return false;
        std::string_view request_line = full_data.substr(0, line_end);

        // 解析 Method, Path
        size_t m_end = request_line.find(' ');
        size_t p_end = request_line.find(' ', m_end + 1);
        if (m_end == std::string_view::npos || p_end == std::string_view::npos) return false;

        req_.method = request_line.substr(0, m_end);
        req_.path = request_line.substr(m_end + 1, p_end - m_end - 1);

        // 2. 逐行解析 Header Fields
        size_t pos = line_end + 2;
        while (pos < total_header_size - 2) {
            // 减 2 是跳过最后的 \r\n
            size_t next_line = full_data.find("\r\n", pos);
            if (next_line == std::string_view::npos) break;

            std::string_view line = full_data.substr(pos, next_line - pos);
            if (line.empty()) break; // 到了 Header 与 Body 之间的空行

            size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                std::string_view key = line.substr(0, colon);
                std::string_view value = line.substr(colon + 1);
                // 去除 value 前导空格
                if (!value.empty() && value[0] == ' ') value.remove_prefix(1);

                // 记录到 Map 中
                req_.headers[key] = value;

                // 特殊处理 Content-Length
                if (key == "Content-Length" || key == "content-length") {
                    std::from_chars(value.data(), value.data() + value.size(), req_.content_length);
                }
            }
            pos = next_line + 2;
        }

        return true;
    }
} // namespace gee
