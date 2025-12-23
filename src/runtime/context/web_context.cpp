#include "runtime/context/web_context.h"

#include <charconv>

#include "runtime/web_io.h"
#include "../src/db/db.h"


#include <string_view>

namespace gee {
    void WebContext::set_params(std::unordered_map<std::string, std::string> params) {
        req_.params_ = std::move(params);
    }

    // 业务层调用：ctx->Param("userId")
    std::string WebContext::Param(const std::string &key) {
        auto it = req_.params_.find(key);
        return it != req_.params_.end() ? it->second : "";
    }

    std::string WebContext::Query(const std::string &key) {
        auto it = req_.query_params_.find(key);
        return (it != req_.query_params_.end()) ? it->second : "";
    }


    void WebContext::send_response(int http_code, const std::string &text) {
        if (res_.is_sent) return; // 状态锁，防止重复发送

        // 1. 让 Response 构造完整的 HTTP 报文流
        std::string http_packet = res_.build_http_packet(http_code, text);
        runtime::web_write(this->fd, http_packet.data(), http_packet.size());
        res_.is_sent = true;
    }


    void WebContext::JSON(gee::StateCode code, const std::string &msg, std::string &&raw_json) {
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
    void WebContext::JSON(gee::StateCode code, const std::string &msg, const db::Model &model) {
        std::string body;
        body.reserve(128);
        model.write_json(body);
        res_.set_raw_data(static_cast<int>(code), msg, std::move(body));
        this->send_response(static_cast<int>(code));
    }

    /**
     * @brief 发送纯文本/HTML 响应 (例如 404)
     */
    void WebContext::String(int http_code, const std::string &text) {
        this->send_response(http_code, text);
    }


    std::string WebContext::PostForm(const std::string &key) {
        auto it = req_.post_form_.find(key);
        return it != req_.post_form_.end() ? it->second : "";
    }

    // 获取原始 Body (用于 JSON 等)
    std::string_view WebContext::Body() {
        return req_.body;
    }
}
