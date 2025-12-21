#pragma once
#include "mysql_driver.h"
#include <queue>
#include <mutex>
#include <string>

namespace db {

    class MySQLPool {
    public:
        // 单例模式，方便全局访问
        static MySQLPool& get() {
            static MySQLPool instance;
            return instance;
        }

        /**
         * @brief 初始化连接池（由主线程在程序启动时调用）
         * @param size 连接池大小
         */
        void init(const std::string& host, const std::string& user,
                  const std::string& pass, const std::string& dbname, int size);

        /**
         * @brief 获取一个连接驱动
         * @return 这里的返回值建议使用 unique_ptr 配合自定义删除器，或者手动管理
         */
        MySQLDriver* acquire();

        /**
         * @brief 归还连接到池中
         */
        void release(MySQLDriver* driver);

        // 获取当前可用连接数
        size_t available() {
            std::lock_guard<std::mutex> lock(mtx_);
            return pool_.size();
        }

    private:
        MySQLPool() = default;
        ~MySQLPool();

        std::queue<MySQLDriver*> pool_;
        std::mutex mtx_;
        // 用于记录总连接数
        int capacity_ = 0;
    };

} // namespace db