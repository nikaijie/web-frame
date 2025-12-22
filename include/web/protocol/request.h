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
        size_t header_size = 0;    // 记录 Header 结束的位置，方便定位 Body
    };
}

