#pragma once
#include "web/protocol/request.h"
#include <charconv>
#include <iostream>
#include "web/protocol/MultipartProcessor.h"
#include "../../../include/pool/io_task_pool.h"
#include "runtime/netpoller.h"

namespace gee {
    bool Request::parse(int client_fd) {
        const size_t MAX_HEADER_SIZE = 8192;
        // 普通请求的限制，文件上传可以单独放开
        const size_t MAX_NORMAL_BODY_SIZE = 10 * 1024 * 1024;

        //   Header
        while (true) {
            size_t header_end_pos = find_header_end();
            if (header_end_pos != std::string_view::npos) {
                this->header_size = header_end_pos + 4;
                break;
            }
            if (raw_data_.size() > MAX_HEADER_SIZE) return false;
            if (web_read(client_fd, this->raw_data_) <= 0) return false;
        }

        if (!do_parse_header(this->header_size)) return false;

        this->content_length = peek_content_length();
        std::string_view ct = get_header("Content-Type");

        //文件的处理
        if (ct.find("multipart/form-data") != std::string_view::npos) {
            return handle_multipart_streaming(client_fd);
        } else {
            // 表单，json的处理
            if (this->content_length > 0) {
                if (this->content_length > MAX_NORMAL_BODY_SIZE) return false;

                // 只有普通请求才预分配内存
                this->raw_data_.reserve(this->header_size + this->content_length);

                // 继续原有的 while 循环读完 Body...
                while (raw_data_.size() < this->header_size + this->content_length) {
                    if (web_read(client_fd, this->raw_data_) <= 0) return false;
                }
                this->body = std::string_view(raw_data_.data() + this->header_size, this->content_length);
                parse_body();
            }
        }
        return true;
    }

    bool Request::handle_multipart_streaming(int client_fd) {
        std::string_view ct = get_header("Content-Type");
        std::string boundary = extract_boundary(ct);
        if (boundary.empty()) return false;

        MultipartProcessor processor(boundary, "./uploads/", &gee::IOTaskPool::instance());

        // 2. 处理初始残留数据
        std::string_view initial_body(raw_data_.data() + header_size, raw_data_.size() - header_size);
        if (!initial_body.empty()) {
            processor.feed(initial_body);
        }

        size_t total_received = initial_body.size();
        std::vector<char> stream_buffer;
        stream_buffer.reserve(128 * 1024); // 适当加大缓冲区，减少 web_read 次数

        while (total_received < content_length) {
            // A. 协程在 web_read 这里会 yield，让出 CPU
            ssize_t n = web_read(client_fd, stream_buffer);
            if (n <= 0) break;

            // B. feed 内部不再执行 current_file_.write()
            // 而是执行 io_pool.addTask(...)
            if (!processor.feed(std::string_view(stream_buffer.data(), stream_buffer.size()))) {
                return false;
            }

            total_received += stream_buffer.size();
            stream_buffer.clear();
        }

        // 告知处理器：数据流结束了，可能需要执行最后的 fsync
        processor.finish();

        this->uploaded_files_ = processor.get_saved_filenames();
        return true;
    }


    std::string Request::extract_boundary(std::string_view content_type) {
        // 寻找 "boundary=" 关键字
        std::string_view target = "boundary=";
        auto pos = content_type.find(target);
        if (pos == std::string_view::npos) {
            return "";
        }

        // 提取 boundary= 之后的所有内容
        std::string_view boundary = content_type.substr(pos + target.size());

        // 防御处理：有些浏览器可能会用引号包裹，如 boundary="----abc"
        if (!boundary.empty() && boundary.front() == '"') {
            boundary.remove_prefix(1);
            if (!boundary.empty() && boundary.back() == '"') {
                boundary.remove_suffix(1);
            }
        }

        return std::string(boundary);
    }


    std::string_view Request::get_header(const std::string &key) const {
        auto it = headers.find(key);
        if (it != headers.end()) {
            return it->second;
        }
        return ""; // 或者返回 std::string_view()
    }

    void Request::parse_body() {
        if (method != "POST" && method != "PUT" && method != "PATCH") return;
        if (body.empty()) return;

        auto it = headers.find("Content-Type");
        if (it == headers.end()) {
            return;
        }
        std::string_view content_type = it->second;

        //  处理传统表单 (application/x-www-form-urlencoded)
        std::cout << content_type << std::endl;
        if (content_type.find("application/x-www-form-urlencoded") != std::string_view::npos) {
            parse_form_urlencoded();
        }
    }

    void Request::parse_form_urlencoded() {
        std::string_view current = body;
        std::cout << "--- [DEBUG] Parsing POST Body ---" << std::endl;
        std::cout << "Raw Body: " << body << std::endl;
        while (!current.empty()) {
            // 1. 找到 & 分隔符
            size_t pair_end = current.find('&');
            std::string_view pair = (pair_end == std::string_view::npos) ? current : current.substr(0, pair_end);

            // 2. 找到 = 分隔符
            size_t sep = pair.find('=');
            if (sep != std::string_view::npos) {
                std::string_view raw_key = pair.substr(0, sep);
                std::string_view raw_val = pair.substr(sep + 1);

                // 3. 存储解析后的键值对 (记得调用之前写好的 url_decode)
                post_form_[url_decode(raw_key)] = url_decode(raw_val);
            }

            // 4. 移动指针到下一个键值对
            if (pair_end == std::string_view::npos) break;
            current.remove_prefix(pair_end + 1);
        }
    }

    std::string Request::url_decode(std::string_view str) {
        std::string res;
        res.reserve(str.size()); // 预分配内存，减少重分配
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '+') {
                res += ' '; // 表单中 '+' 通常代表空格
            } else if (str[i] == '%' && i + 2 < str.size()) {
                // 解析 %XX 十六进制字符
                char high = str[i + 1];
                char low = str[i + 2];

                // 将十六进制转为整数
                auto hex_to_int = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return 0;
                };

                res += static_cast<char>((hex_to_int(high) << 4) | hex_to_int(low));
                i += 2; // 跳过已解析的两个十六进制字符
            } else {
                res += str[i];
            }
        }
        return res;
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
                std::string_view key_view = line.substr(0, colon);
                std::string_view value_view = line.substr(colon + 1);
                auto trim = [](std::string_view s) {
                    size_t first = s.find_first_not_of(" \t\r\n");
                    if (first == std::string_view::npos) return std::string_view();
                    size_t last = s.find_last_not_of(" \t\r\n");
                    return s.substr(first, (last - first + 1));
                };

                std::string key = std::string(trim(key_view));
                std::string value = std::string(trim(value_view));

                this->headers[key] = value;
                if (strcasecmp(key.c_str(), "Content-Length") == 0) {
                    std::from_chars(value.data(), value.data() + value.size(), this->content_length);
                }
            }
            pos = next_line + 2;
        }

        return true;
    }

    ssize_t Request::web_read(int fd, std::vector<char> &buffer) {
        char temp[4096];
        ssize_t n = ::read(fd, temp, sizeof(temp));
        if (n > 0) {
            buffer.insert(buffer.end(), temp, temp + n);
            return n;
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // 关键点：数据还没到，挂起协程
            auto g = runtime::Goroutine::current();
            runtime::Netpoller::get().watch_read_web(fd, g);
            runtime::Goroutine::yield();

            // 被唤醒后，递归调一次自己，去读新到的数据
            return web_read(fd, buffer);
        }
        return n; // 0 或 -1 (错误)
    }
}
