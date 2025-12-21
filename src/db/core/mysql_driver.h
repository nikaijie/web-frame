#pragma once
#include <mysql.h>
#include <string>
#include <vector>
#include <map>

namespace db {
    using RowData = std::map<std::string, std::string>;

    class MySQLDriver {
    public:
        MySQLDriver();

        ~MySQLDriver();

        // 禁用拷贝
        MySQLDriver(const MySQLDriver &) = delete;

        MySQLDriver &operator=(const MySQLDriver &) = delete;

        /**
         * @brief 同步连接：仅限主线程启动时调用
         */
        bool connect_sync(const std::string &host, const std::string &user,
                          const std::string &pass, const std::string &dbname);

        /**
         * @brief 异步查询：由协程调用，内部会 yield
         */
        std::vector<RowData> execute_query(const std::string &sql);

        /**
         * @brief 异步更新：由协程调用
         */
        int execute_update(const std::string &sql);

    private:
        void wait_io(int status);

        std::vector<RowData> parse_result(MYSQL_RES *res);

        MYSQL *mysql_;
    };
} // namespace db
