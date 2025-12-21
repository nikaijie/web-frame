#include "web/core/gee.h"
#include "runtime/goroutine.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>

#include "runtime/context/web_context.h"

namespace gee {
    void Engine::add_route(std::string method, std::string path, HandlerFunc handler) {
        std::string key = method + "-" + path;
        routes_[key] = std::move(handler);
    }

    void Engine::Run(int port) {
        int listen_fd = create_listen_socket(port);
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &client_len);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EINTR) continue;
                break;
            }

            // 設置非阻塞，這是協程 IO 能正常運作的前提
            fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL) | O_NONBLOCK);
            handle_http_task(client_fd);
        }
    }

    void Engine::handle_http_task(int client_fd) {
        runtime::go([this, client_fd]() {
            gee::WebContext ctx(client_fd);

            if (ctx.parse()) {
                std::string key = std::string(ctx.method()) + "-" + std::string(ctx.path());
                if (routes_.count(key)) {
                    // 业务代码示例：
                    // routes_[key] 内部可以写：ctx->res_.set_result(0, "success", "{\"id\":1}");
                    routes_[key](&ctx);

                    // 统一调用出口：不传 text，让 String 自动去序列化 res_
                    ctx.String(200, "");
                } else {
                    ctx.String(404, "{\"state\":404, \"message\":\"Not Found\"}");
                }
            } else {
                ctx.String(400, "{\"state\":400, \"message\":\"Bad Request\"}");
            }

            ::close(client_fd);
        });
    }

    int Engine::create_listen_socket(int port) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(fd, (struct sockaddr *) &addr, sizeof(addr));
        listen(fd, 1024);
        return fd;
    }
}
