#pragma once
#include <string>
#include <sstream>

namespace web {
    class Response {
    private:
        int status_code_ = 200;
        std::string content_type_ = "text/plain";
        std::stringstream body_;

    public:
        template <typename T>
        Response& operator<<(const T& data) {
            body_ << data;
            return *this;
        }

        std::string ToString() const;
    };
}
