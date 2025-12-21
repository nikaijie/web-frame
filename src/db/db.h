#pragma once
#include "core/mysql_pool.h"
#include "orm/query_builder.h"
#include "orm/model.h"


namespace db {
    /**
     * @brief 框架初始化函数 (建议在 main 函数开头调用)
     */
    inline void init(const std::string &host, const std::string &user,
                     const std::string &pass, const std::string &dbname, int pool_size) {
        MySQLPool::get().init(host, user, pass, dbname, pool_size);
    }
} // namespace db
