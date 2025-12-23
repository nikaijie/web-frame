#pragma once
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gee {
    struct Request {
        std::string_view method;
        std::string_view path;
        std::unordered_map<std::string_view, std::string_view> headers;
        std::string_view body;

        size_t content_length = 0; // Body 长度
        size_t header_size = 0; // 记录 Header 结束的位置，方便定位 Body

        std::unordered_map<std::string, std::string> params_; //存放user/:name/:age后面的参数
        std::unordered_map<std::string, std::string> query_params_; //存放？name=xx&age=18 后面的参数

        std::vector<char> raw_data_ = {}; //io拷贝时的数组

        bool parse(int client_fd);

        size_t peek_content_length();

        size_t find_header_end();

        bool do_parse_header(size_t total_header_size);

        void parse_query_string(std::string_view query);
    };
}
