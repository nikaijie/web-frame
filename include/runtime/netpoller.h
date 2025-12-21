#pragma once
#include <sys/event.h>
#include <map>
#include <memory>
#include <string>
#include "runtime/goroutine.h"
#include "runtime/IOContext.h"

namespace runtime {

    enum class IOEvent {
        Read = EVFILT_READ,
        Write = EVFILT_WRITE
    };

    class Netpoller {
    public:
        static Netpoller& get();
        Netpoller();
        ~Netpoller();

        // 通用监听：支持 Read 或 Write
        void watch(int fd, IOEvent event, Goroutine::Ptr g);

        // 原有的业务接口可以保留作为快捷方式
        void watch_read(int fd, Goroutine::Ptr g);

        void poll_loop();

        // 辅助方法：获取缓冲区数据
        std::string fetch_read_buf(int fd);

    private:
        int kq_fd_;
        // 必须使用 mutex 保护 contexts_，因为 watch 可能由不同 Worker 线程调用
        std::mutex mtx_;
        std::map<int, std::unique_ptr<IOContext>> contexts_;
    };

} // namespace runtime