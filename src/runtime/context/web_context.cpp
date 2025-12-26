#include "runtime/context/web_context.h"
#include <charconv>
#include "../src/db/db.h"


#include <string_view>

#include "runtime/netpoller.h"

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
        web_write(this->fd, http_packet.data(), http_packet.size());
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

    ssize_t WebContext::web_write(int fd, const char* data, size_t len) {
        size_t total_sent = 0;

        while (total_sent < len) {
            ssize_t n = ::write(fd, data + total_sent, len - total_sent);
            if (n > 0) {
                total_sent += n;
                // 继续循环，尝试写剩下的部分
            } else if (n == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 内核发送缓冲区满了，注册可写事件并挂起
                    auto g = runtime::Goroutine::current();
                    // 注意这里使用的是 IOEvent::Write
                    runtime::Netpoller::get().watch(fd, runtime::IOEvent::Write, g);

                    runtime::Goroutine::yield();
                    // 被唤醒后，说明现在可以继续写了，回到 while 循环
                    continue;
                }
                if (errno == EINTR) continue;
                return -1; // 真正的 Socket 错误
            } else {
                return 0; // 对端关闭
            }
        }
        return total_sent;
    }
}
