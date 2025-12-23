#pragma once

#include "web/protocol/request.h"

#include <charconv>

#include "runtime/web_io.h"

namespace gee {
    bool Request::parse(int client_fd) {
        const size_t MAX_HEADER_SIZE = 8192; // 8KB
        const size_t MAX_BODY_SIZE = 10 * 1024 * 1024; // 10MB

        // --- 读 Header ---
        while (true) {
            size_t header_end_pos = find_header_end();
            if (header_end_pos != std::string_view::npos) {
                // 找到了 \r\n\r\n
                if (header_end_pos > MAX_HEADER_SIZE) return false;
                this->header_size = header_end_pos + 4;

                // 偷窥 Content-Length
                this->content_length = peek_content_length();
                break;
            }
            // 防御长 Header 攻击：即使没读完，超过 8KB 也要熔断
            if (raw_data_.size() > MAX_HEADER_SIZE) return false;
            if (runtime::web_read(client_fd, this->raw_data_) <= 0) return false;
        }

        // ---  预留内存与 Body 大小防御 ---
        if (this->content_length > 0) {
            // ：防御超大 Body
            if (this->content_length > MAX_BODY_SIZE) {
                return false;
            }

            //  预分配raw_data_
            this->raw_data_.reserve(this->header_size + this->content_length);
        }

        if (!do_parse_header(this->header_size)) return false;

        //  读 Body ---
        if (this->content_length > 0) {
            while (raw_data_.size() < this->header_size + this->content_length) {
                if (runtime::web_read(client_fd, this->raw_data_) <= 0) return false;
            }
            // 地址固定，直接绑定 Body
            this->body = std::string_view(raw_data_.data() + this->header_size, this->content_length);
        }

        return true;
    }


    size_t Request::peek_content_length() {
        // 将已读到的 Header 数据转为 string_view 方便查找
        std::string_view header_view(raw_data_.data(), raw_data_.size());

        // 查找 "Content-Length:" 关键字 (忽略大小写)
        // 注意：实际开发中 Content-Length 可能大小写混合，这里简单演示
        static const std::string target = "Content-Length:";
        size_t pos = 0;

        // 简单的查找逻辑
        // 提示：生产环境建议用更严谨的忽略大小写匹配
        pos = header_view.find(target);
        if (pos == std::string_view::npos) {
            // 再试一下小写版本
            pos = header_view.find("content-length:");
        }

        if (pos != std::string_view::npos) {
            size_t val_start = pos + target.size();
            // 找到数字的起始位置（跳过空格）
            while (val_start < header_view.size() && header_view[val_start] == ' ') {
                val_start++;
            }

            // 找到数字的结束位置 (\r)
            size_t val_end = header_view.find("\r", val_start);
            if (val_end != std::string_view::npos) {
                std::string_view val_sv = header_view.substr(val_start, val_end - val_start);

                // 使用 charconv 快速转数字，或者直接用 atoi
                size_t length = 0;
                auto [ptr, ec] = std::from_chars(val_sv.data(), val_sv.data() + val_sv.size(), length);
                if (ec == std::errc()) {
                    return length;
                }
            }
        }
        return 0; // 没找到或解析失败，默认为 0
    }


    // 辅助：查找 \r\n\r\n
    size_t Request::find_header_end() {
        std::string_view sv(raw_data_.data(), raw_data_.size());
        return sv.find("\r\n\r\n");
    }

    void Request::parse_query_string(std::string_view query) {
        size_t pos = 0;
        while (pos < query.size()) {
            size_t ampersand = query.find('&', pos);
            std::string_view pair = (ampersand == std::string_view::npos)
                                        ? query.substr(pos)
                                        : query.substr(pos, ampersand - pos);

            size_t equal = pair.find('=');
            if (equal != std::string_view::npos) {
                std::string_view key_sv = pair.substr(0, equal);
                std::string_view val_sv = pair.substr(equal + 1);

                // 存入 Request 对象的 query_params map 中
                this->query_params_[std::string(key_sv)] = std::string(val_sv);
            }

            if (ampersand == std::string_view::npos) break;
            pos = ampersand + 1;
        }
    }

    bool Request::do_parse_header(size_t total_header_size) {
        this->header_size = total_header_size;
        std::string_view full_data(raw_data_.data(), total_header_size);

        // --- 1. 解析请求行 (e.g., "GET /user?id=1 HTTP/1.1") ---
        size_t line_end = full_data.find("\r\n");
        if (line_end == std::string_view::npos) return false;
        std::string_view request_line = full_data.substr(0, line_end);

        size_t m_end = request_line.find(' ');
        size_t p_end = request_line.find(' ', m_end + 1);
        if (m_end == std::string_view::npos || p_end == std::string_view::npos) return false;

        this->method = request_line.substr(0, m_end);

        // 关键逻辑：切分 Path 和 Query String
        std::string_view full_path = request_line.substr(m_end + 1, p_end - m_end - 1);
        size_t question_mark = full_path.find('?');
        if (question_mark != std::string_view::npos) {
            this->path = full_path.substr(0, question_mark); // 纯净路径给 Trie 树
            std::string_view query_str = full_path.substr(question_mark + 1);
            parse_query_string(query_str); // 解析参数
        } else {
            this->path = full_path;
        }

        // --- 2. 逐行解析 Header Fields ---
        size_t pos = line_end + 2;
        while (pos < total_header_size) {
            size_t next_line = full_data.find("\r\n", pos);
            if (next_line == std::string_view::npos) break;

            std::string_view line = full_data.substr(pos, next_line - pos);
            if (line.empty()) break; // 遇到空行说明 Header 结束

            size_t colon = line.find(':');
            if (colon != std::string_view::npos) {
                std::string_view key = line.substr(0, colon);
                std::string_view value = line.substr(colon + 1);

                // 去除前面的空格
                while (!value.empty() && value[0] == ' ') value.remove_prefix(1);

                this->headers[std::string(key)] = std::string(value);

                // 提取 Content-Length
                if (key == "Content-Length" || key == "content-length") {
                    std::from_chars(value.data(), value.data() + value.size(), this->content_length);
                }
            }
            pos = next_line + 2;
        }

        return true;
    }
}
