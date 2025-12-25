#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

void init_logging() {
    spdlog::init_thread_pool(8192, 1);

    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("gee_server.log", true);

    std::vector<spdlog::sink_ptr> sinks{stdout_sink, file_sink};

    // 2. 创建异步 Logger
    auto logger = std::make_shared<spdlog::async_logger>(
        "gee_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);

    spdlog::set_default_logger(logger);

    // 3. 设置日志格式（包含线程 ID，方便调试协程切换）
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v");
}
