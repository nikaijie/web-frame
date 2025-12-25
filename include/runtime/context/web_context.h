#pragma once
#include "iocontext.h"
#include "web/protocol/request.h"
#include "web/protocol/response.h"
#include <string>
#include <stdexcept>
#include "../src/db/db.h"

namespace gee {
    class WebContext;
    using HandlerFunc = std::function<void(WebContext *)>;

    struct WebContext : public runtime::IOContextBase {
        gee::Request req_;
        gee::Response res_;

        std::vector<HandlerFunc> handlers_;
        // 记录当前执行到了第几个 Handler，初始为 -1
        int index_;

        WebContext(int f)
            : runtime::IOContextBase(f, runtime::IOType::WEB), index_(-1) {
        }

        void Next() {
            index_++;
            int s = static_cast<int>(handlers_.size());
            for (; index_ < s; index_++) {
                handlers_[index_](this);
            }
        }

        void Abort() {
            index_ = static_cast<int>(handlers_.size());
        }

        void set_params(std::unordered_map<std::string, std::string> params);

        // 业务层调用：ctx->Param("userId")
        std::string Param(const std::string &key);

        std::string Query(const std::string &key);


        void send_response(int http_code, const std::string &text = "");

        void JSON(gee::StateCode code, const std::string &msg, std::string &&raw_json);


        /**
         * @brief 处理单个 Model 对象
         */
        void JSON(gee::StateCode code, const std::string &msg, const db::Model &model);

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
        void String(int http_code, const std::string &text);

        // 获取请求信息
        std::string_view method() const { return req_.method; }
        std::string_view path() const { return req_.path; }


        //post表单
        std::string PostForm(const std::string &key);

        // 获取原始 Body (用于 JSON 等)
        std::string_view Body();
    };
}
