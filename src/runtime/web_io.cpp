#include "runtime/web_io.h"
#include "runtime/netpoller.h"
#include "runtime/scheduler.h"
#include <unistd.h>

namespace runtime {
    //web_io读函数
    ssize_t web_read(int fd, std::vector<char>& buffer) {
        char temp[4096];
        ssize_t n = ::read(fd, temp, sizeof(temp));
        if (n > 0) {
            buffer.insert(buffer.end(), temp, temp + n);
            return n;
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            // 关键点：数据还没到，挂起协程
            auto g = Goroutine::current();
            Netpoller::get().watch_read_web(fd, g);
            Goroutine::yield();

            // 被唤醒后，递归调一次自己，去读新到的数据
            return web_read(fd, buffer);
        }
        return n; // 0 或 -1 (错误)
    }

    //web函数写函数
    ssize_t web_write(int fd, const char* data, size_t len) {
        size_t total_sent = 0;

        while (total_sent < len) {
            ssize_t n = ::write(fd, data + total_sent, len - total_sent);

            if (n > 0) {
                total_sent += n;
                // 继续循环，尝试写剩下的部分
            } else if (n == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 内核发送缓冲区满了，注册可写事件并挂起
                    auto g = Goroutine::current();
                    // 注意这里使用的是 IOEvent::Write
                    Netpoller::get().watch(fd, IOEvent::Write, g);

                    Goroutine::yield();
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
} // namespace runtime
