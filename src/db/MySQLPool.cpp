#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <mysql.h>
#include "mysql_coro.cpp"

class MySQLPool {
public:
    static MySQLPool& get() {
        static MySQLPool instance;
        return instance;
    }

    // 主线程调用：预先创建固定数量的连接
    void init(const char* host, const char* user, const char* pass, const char* db, int size) {
        db::MySQLCoro connector;
        for (int i = 0; i < size; ++i) {
            // 注意：这里使用 async=false，因为主线程没有协程环境
            MYSQL* conn = connector.connect(host, user, pass, db, false);
            if (conn) {
                pool_.push(conn);
            }
        }
        std::cout << "连接池初始化完成，可用连接: " << pool_.size() << std::endl;
    }

    // 协程调用：获取连接
    MYSQL* acquire() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (pool_.empty()) return nullptr;
        MYSQL* conn = pool_.front();
        pool_.pop();
        return conn;
    }

    // 协程调用：归还连接
    void release(MYSQL* conn) {
        std::lock_guard<std::mutex> lock(mtx_);
        pool_.push(conn);
    }

private:
    std::queue<MYSQL*> pool_;
    std::mutex mtx_;
};