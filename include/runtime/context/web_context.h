#pragma once
#include "iocontext.h"
#include "web/protocol/request.h"
#include "web/protocol/response.h"
#include <string>
#include <stdexcept>
#include "../src/db/db.h"
#include "runtime/web_io.h"

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

        void parse_query_string(std::string_view query);

        void set_params(std::unordered_map<std::string, std::string> params) {
            req_.params_ = std::move(params);
        }

        // 业务层调用：ctx->Param("userId")
        std::string Param(const std::string &key) {
            auto it = req_.params_.find(key);
            return it != req_.params_.end() ? it->second : "";
        }

        std::string Query(const std::string &key) {
            auto it = req_.query_params_.find(key);
            return (it != req_.query_params_.end()) ? it->second : "";
        }


        void send_response(int http_code, const std::string &text = "") {
            if (res_.is_sent) return; // 状态锁，防止重复发送

            // 1. 让 Response 构造完整的 HTTP 报文流
            std::string http_packet = res_.build_http_packet(http_code, text);
            runtime::web_write(this->fd, http_packet.data(), http_packet.size());
            res_.is_sent = true;
        }


        void JSON(gee::StateCode code, const std::string &msg, std::string &&raw_json) {
            if (!raw_json.empty()) {
                char first = raw_json.front();
                char last = raw_json.back();

                // 扩展校验：允许 { } , [ ] , 以及 " "
                bool is_obj = (first == '{' && last == '}');
                bool is_arr = (first == '[' && last == ']');
                bool is_str = (first == '\"' && last == '\"'); // 新增：允许 JSON 字符串格式

                if (!is_obj && !is_arr && !is_str) {
                    throw std::invalid_argument(
                        "Invalid JSON fragment: data must be wrapped in {}, [] or \"\"."
                    );
                }
            } else {
                raw_json = "{}";
            }
            res_.set_raw_data(static_cast<int>(code), msg, std::move(raw_json));
            this->send_response(200);
        }


        /**
         * @brief 处理单个 Model 对象
         */
        void JSON(gee::StateCode code, const std::string &msg, const db::Model &model) {
            std::string body;
            body.reserve(128);
            model.write_json(body);
            res_.set_raw_data(static_cast<int>(code), msg, std::move(body));
            this->send_response(static_cast<int>(code));
        }


        /**
         * @brief 处理对象数组 (vector)
         */
        template<typename T>
        typename std::enable_if<std::is_base_of<db::Model, T>::value>::type
        JSON(gee::StateCode code, const std::string &msg, const std::vector<T> &models) {
            std::string body;
            if (models.empty()) {
                body = "[]";
            } else {
                // 极致性能：预分配内存 (按你之前的 120 字节估算)
                body.reserve(models.size() * 120 + 2);
                body.push_back('[');
                for (size_t i = 0; i < models.size(); ++i) {
                    models[i].write_json(body);
                    if (i + 1 < models.size()) body.push_back(',');
                }
                body.push_back(']');
            }
            res_.set_raw_data(static_cast<int>(code), msg, std::move(body));
            this->send_response(static_cast<int>(code));
        }

        /**
         * @brief 发送纯文本/HTML 响应 (例如 404)
         */
        void String(int http_code, const std::string &text) {
            this->send_response(http_code, text);
        }

        // 获取请求信息
        std::string_view method() const { return req_.method; }
        std::string_view path() const { return req_.path; }
    };
}
