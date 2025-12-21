#include "runtime/netpoller.h"
#include "runtime/scheduler.h"
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

namespace runtime {

    Netpoller& Netpoller::get() {
        static Netpoller instance;
        return instance;
    }

    Netpoller::Netpoller() {
        kq_fd_ = kqueue();
    }

    Netpoller::~Netpoller() {
        if (kq_fd_ != -1) close(kq_fd_);
    }

    void Netpoller::watch(int fd, IOEvent event, Goroutine::Ptr g) {
        // 1. 设置非阻塞
        int flags = fcntl(fd, F_GETFL, 0);
        if (!(flags & O_NONBLOCK)) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        {
            lock_.lock();
            if (contexts_.find(fd) == contexts_.end()) {
                contexts_[fd] = std::make_unique<IOContext>(fd);
            }
            contexts_[fd]->waiting_g = std::move(g);
            lock_.unlock();
        }

        // 3. 注册 kqueue 事件

        struct kevent ev;
        // 使用 EV_ONESHOT：触发一次后自动删除，非常适合异步驱动的状态机切换
        EV_SET(&ev, fd, static_cast<int16_t>(event), EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, contexts_[fd].get());

        if (kevent(kq_fd_, &ev, 1, nullptr, 0, nullptr) == -1) {
            perror("kevent watch failed");
        }
    }

    void Netpoller::watch_read(int fd, Goroutine::Ptr g) {
        watch(fd, IOEvent::Read, std::move(g));
    }

    void Netpoller::poll_loop() {
        struct kevent events[1024];
        while (true) {

            int n = kevent(kq_fd_, nullptr, 0, events, 1024, nullptr);
            if (n <= 0) continue;
            for (int i = 0; i < n; ++i) {
                IOContext* ctx = static_cast<IOContext*>(events[i].udata);
                if (!ctx) continue;

                // 处理读事件
                // if (events[i].filter == EVFILT_READ) {
                //     std::cout<<"poll_loop()_read"<<std::endl;
                //     char tmp[8192];
                //     while (true) {
                //         ssize_t bytes = read(ctx->fd, tmp, sizeof(tmp));
                //         if (bytes > 0) {
                //             ctx->read_buf.append(tmp, bytes);
                //         } else {
                //             break;
                //         }
                //     }
                // }

                // 处理写事件 (或者读事件读完后)
                // 只要事件触发，说明 fd 就绪，唤醒协程
                std::lock_guard<std::mutex> lock(mtx_);
                if (ctx->waiting_g) {
                    Scheduler::get().push_ready(std::move(ctx->waiting_g));
                    ctx->waiting_g = nullptr;
                }
            }
        }
    }

    std::string Netpoller::fetch_read_buf(int fd) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (contexts_.count(fd)) {
            std::string data = std::move(contexts_[fd]->read_buf);
            contexts_[fd]->read_buf.clear();
            return data;
        }
        return "";
    }

} // namespace runtime