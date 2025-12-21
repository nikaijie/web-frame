#pragma once
#include <string>
#include <vector>
#include "runtime/goroutine.h"

namespace runtime {

    // 每个连接独有的 IO 上下文
    struct IOContext {
        int fd;
        std::string read_buf;  // 读缓冲区
        std::string write_buf; // 写缓冲区
        Goroutine::Ptr waiting_g; // 正在等待此 IO 的协程
        size_t expected_size = 0;

        IOContext(int f) : fd(f) {}
    };

} // namespace runtime