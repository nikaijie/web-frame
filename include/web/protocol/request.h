#pragma once
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gee {
    struct Request {
        std::string_view method;
        std::string_view path;
        std::unordered_map<std::string, std::string> headers;
        std::string_view body;

        size_t content_length = 0; // Body 长度
        size_t header_size = 0; // 记录 Header 结束的位置，方便定位 Body

        std::unordered_map<std::string, std::string> post_form_; // 存放 POST 表单数据 (application/x-www-form-urlencoded)

        std::unordered_map<std::string, std::string> params_; //存放user/:name/:age后面的参数
        std::unordered_map<std::string, std::string> query_params_; //存放？name=xx&age=18 后面的参数

        std::vector<char> raw_data_ = {}; //io拷贝时的数组

        // 存放 JSON 对象 (建议集成 nlohmann/json，或者先存为原始 string)
        std::string json_body_;

        bool parse(int client_fd);

        size_t peek_content_length();

        size_t find_header_end();

        bool do_parse_header(size_t total_header_size);

        void parse_query_string(std::string_view query);

        std::vector<std::string> uploaded_files_; // 存储已保存的文件完整路径

        void parse_body(); //解析body
        std::string url_decode(std::string_view str); //URL 解码逻辑

        void parse_form_urlencoded();

        bool handle_multipart_streaming(int client_fd);

        std::string extract_boundary(std::string_view content_type);

        std::string_view get_header(const std::string &key) const;


        const std::vector<std::string> &get_uploaded_files() const {
            return uploaded_files_;
        }
    };
}
