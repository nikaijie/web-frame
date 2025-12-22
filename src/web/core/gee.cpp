#include "web/core/gee.h"
#include "runtime/goroutine.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <spdlog/spdlog.h>

#include "runtime/context/web_context.h"

namespace gee {



    void Engine::Run(int port) {
        int listen_fd = create_listen_socket(port);
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(listen_fd, (struct sockaddr *) &client_addr, &client_len);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EINTR) continue;
                spdlog::error("client_fd appear panic");
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
                auto [node, params] = get_route(std::string(ctx.method()), std::string(ctx.path()));
                if (node) {
                    ctx.set_params(std::move(params)); // 记得给 WebContext 加上这个方法
                    std::string key = std::string(ctx.method()) + "-" + node->pattern;
                    if (routes_.count(key)) {
                        routes_[key](&ctx);
                        ctx.String(200, "");
                    } else {
                        ctx.String(400, "{\"state\":400, \"message\":\"Bad Request\"}");
                    }
                } else {
                    ctx.String(404, "404 Not Found");
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
