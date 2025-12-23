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
            try {
                if (ctx.parse()) {
                    auto [node, params] = get_route(std::string(ctx.method()), std::string(ctx.path()));
                    if (node) {
                        ctx.set_params(std::move(params));
                        std::string key = std::string(ctx.method()) + "-" + node->pattern;
                        if (routes_.count(key)) {
                            routes_[key](&ctx);
                            if (!ctx.res_.is_sent) {
                                ctx.JSON(gee::StateCode::OK, "OK","{}");
                            }
                        } else {
                            ctx.JSON(gee::StateCode::SERVER_ERROR, "Handler Missing", "{}");
                        }
                    } else {
                        ctx.JSON(gee::StateCode::NOT_FOUND, "404 Not Found", "{}");
                    }
                } else {
                    ctx.JSON(gee::StateCode::PARAM_ERROR, "Invalid HTTP Request", "{}");
                }
            }catch (std::exception& e) {
                spdlog::error(e.what());
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
