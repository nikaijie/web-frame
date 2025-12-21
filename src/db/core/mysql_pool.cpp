#include "mysql_pool.h"
#include <iostream>

#include "runtime/goroutine.h"

namespace db {
    void MySQLPool::init(const std::string &host, const std::string &user,
                         const std::string &pass, const std::string &dbname, int size) {
        std::lock_guard<std::mutex> lock(mtx_);
        for (int i = 0; i < size; ++i) {
            auto *driver = new MySQLDriver();
            // 调用我们之前写的同步连接函数
            if (driver->connect_sync(host, user, pass, dbname)) {
                pool_.push(driver);
            } else {
                std::cerr << "MySQLPool: Failed to connect index " << i << std::endl;
                delete driver;
            }
        }
        capacity_ = (int) pool_.size();
        std::cout << "MySQLPool: Initialized with " << capacity_ << " connections." << std::endl;
    }

    MySQLDriver *MySQLPool::acquire() {
        lock_.lock(); // 绝对阻塞，直到拿到锁，不会返回 nullptr

        if (pool_.empty()) {
            lock_.unlock();
            return nullptr; // 只有在池子真的空了时才返回 null
        }

        MySQLDriver *drv = pool_.front();
        pool_.pop();

        lock_.unlock();
        return drv;
    }

    void MySQLPool::release(MySQLDriver *driver) {
        if (!driver) return;

        lock_.lock(); // 绝对阻塞，拿到锁再进去
        pool_.push(driver);
        lock_.unlock();
    }

    MySQLPool::~MySQLPool() {
        while (!pool_.empty()) {
            delete pool_.front();
            pool_.pop();
        }
    }
} // namespace db
