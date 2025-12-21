#include "runtime/netpoller.h"
#include "runtime/scheduler.h"
#include <unistd.h>
#include <fcntl.h>
#include "runtime/context/db_context.h"
#include "runtime/context/web_context.h"

namespace runtime {
    Netpoller &Netpoller::get() {
        static Netpoller instance;
        return instance;
    }

    Netpoller::Netpoller() {
        kq_fd_ = kqueue();
    }

    Netpoller::~Netpoller() {
        if (kq_fd_ != -1) close(kq_fd_);
    }


    void Netpoller::poll_loop() {
        struct kevent events[1024];
        while (true) {
            int n = kevent(kq_fd_, nullptr, 0, events, 1024, nullptr);
            if (n <= 0) continue;
            for (int i = 0; i < n; ++i) {
                auto *ctx = static_cast<runtime::IOContextBase *>(events[i].udata);
                if (!ctx) continue;
                runtime::Goroutine::Ptr g_to_wake;
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    if (ctx->waiting_g) {
                        g_to_wake = std::move(ctx->waiting_g);
                        ctx->waiting_g = nullptr;
                    }
                }
                if (g_to_wake) {
                    Scheduler::get().push_ready(std::move(g_to_wake));
                }
            }
        }
    }


    void Netpoller::watch_read_web(int fd, Goroutine::Ptr g) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (contexts_.find(fd) == contexts_.end()) {
                contexts_[fd] = std::make_unique<gee::WebContext>(fd);
            }
            contexts_[fd]->type = IOType::WEB; // 核心：打上 WEB 标签
            contexts_[fd]->waiting_g = std::move(g);
        }

        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, contexts_[fd].get());
        kevent(kq_fd_, &ev, 1, nullptr, 0, nullptr);
    }

    void Netpoller::watch_write_web(int fd, Goroutine::Ptr g) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (contexts_.find(fd) == contexts_.end()) {
                contexts_[fd] = std::make_unique<gee::WebContext>(fd);
            }
            contexts_[fd]->type = IOType::WEB;
            contexts_[fd]->waiting_g = std::move(g);
        }

        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, contexts_[fd].get());
        kevent(kq_fd_, &ev, 1, nullptr, 0, nullptr);
    }

    //数据库io时向
    void Netpoller::watch(int fd, IOEvent event, Goroutine::Ptr g) {
        // 1. 设置非阻塞
        int flags = fcntl(fd, F_GETFL, 0);
        if (!(flags & O_NONBLOCK)) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        {
            lock_.lock();
            if (contexts_.find(fd) == contexts_.end()) {
                contexts_[fd] = std::make_unique<DBContext>(fd);
            }
            contexts_[fd]->waiting_g = std::move(g);
            lock_.unlock();
        }

        // 3. 注册 kqueue 事件
        struct kevent ev;
        EV_SET(&ev, fd, static_cast<int16_t>(event), EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, contexts_[fd].get());

        if (kevent(kq_fd_, &ev, 1, nullptr, 0, nullptr) == -1) {
            perror("kevent watch failed");
        }
    }

    void Netpoller::watch_read(int fd, Goroutine::Ptr g) {
        watch(fd, IOEvent::Read, std::move(g));
    }
}
