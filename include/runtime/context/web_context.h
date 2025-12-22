#pragma once
#include "iocontext.h"
#include "web/protocol/request.h"
#include "web/protocol/response.h"

namespace gee {
    struct WebContext : public runtime::IOContextBase {
        gee::Request req_;
        gee::Response res_;
        std::vector<char> raw_data_;

        WebContext(int f, std::vector<char> data = {})
            : runtime::IOContextBase(f, runtime::IOType::WEB),
              raw_data_(std::move(data)) {
        }

        // 解析原始字节流
        bool parse();

        size_t find_header_end();

        bool do_parse_header(size_t total_header_size);



        // 业务接口
        void JSON(int code, const std::string &json_str);

        void String(int code, const std::string &text);

        // 获取请求信息
        std::string_view method() const { return req_.method; }
        std::string_view path() const { return req_.path; }
    };
}
