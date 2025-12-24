#ifndef IO_TASK_POOL_H
#define IO_TASK_POOL_H

#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

namespace gee {
    struct IOTask {
        std::string path;
        std::string data;
        bool should_fsync;
    };

    class IOTaskPool {
    public:
        static IOTaskPool &instance() {
            static IOTaskPool pool(1);
            return pool;
        }

        // 禁用拷贝构造和赋值，确保单例唯一性
        IOTaskPool(const IOTaskPool &) = delete;

        IOTaskPool &operator=(const IOTaskPool &) = delete;

        //  Worker 协程调用
        void addTask(std::string path, std::string &&data, bool fsync = false) {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                tasks_.push({std::move(path), std::move(data), fsync});
            }
            condition_.notify_one();
        }

        // 获取当前队列积压的任务数，用于背压控制
        size_t queueSize() {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            return tasks_.size();
        }

        ~IOTaskPool() {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                stop_ = true;
            }
            condition_.notify_all();
            for (std::thread &worker: workers_) {
                if (worker.joinable()) worker.join();
            }
        }

    private:
        // 构造函数私有化
        explicit IOTaskPool(size_t thread_count) : stop_(false) {
            for (size_t i = 0; i < thread_count; ++i) {
                workers_.emplace_back([this] {
                    while (true) {
                        IOTask task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex_);
                            this->condition_.wait(lock, [this] {
                                return this->stop_ || !this->tasks_.empty();
                            });

                            if (this->stop_ && this->tasks_.empty()) return;

                            task = std::move(this->tasks_.front());
                            this->tasks_.pop();
                        }
                        execute_task(task);
                    }
                });
            }
        }

        void execute_task(const IOTask &task) {
            namespace fs = std::filesystem;
            std::error_code ec;

            // 1. 强制转换成绝对路径，彻底消除“相对路径”在哪里的困惑
            fs::path abs_path = fs::absolute(task.path);
            fs::path dir = abs_path.parent_path();

            // 2. 自动建目录：既然只有一个线程，这里绝对不会有冲突
            if (!fs::exists(dir)) {
                fs::create_directories(dir, ec);
                if (ec) {
                    std::cerr << "[IO_ERROR] 无法创建目录: " << dir << " | 原因: " << ec.message() << std::endl;
                    return;
                }
                std::cout << "[IO_DEBUG] 成功创建目录: " << dir << std::endl;
            }

            // 3. 写入逻辑
            // 关键：观察这里打印出来的路径，那是文件在磁盘上的真实坐标！
            std::cout << "[IO_DEBUG] 正在写入文件: " << abs_path << " | 大小: " << task.data.size() << " bytes" << std::endl;

            std::ofstream ofs(abs_path, std::ios::binary | std::ios::app);
            if (ofs.is_open()) {
                ofs.write(task.data.data(), static_cast<std::streamsize>(task.data.size()));
                if (task.should_fsync) {
                    ofs.flush();
                }
                ofs.close();
                std::cout << "[IO_DEBUG] 写入完成 (Success)!" << std::endl;
            } else {
                // 如果失败，打印出系统级别的错误码
                std::cerr << "[IO_ERROR] 无法打开文件: " << abs_path << " | 错误原因: " << strerror(errno) << std::endl;
            }
        }

        std::vector<std::thread> workers_;
        std::queue<IOTask> tasks_;
        std::mutex queue_mutex_;
        std::condition_variable condition_;
        bool stop_;
    };
} // namespace gee

#endif
