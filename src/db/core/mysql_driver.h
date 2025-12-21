#pragma once
#include <mysql.h>
#include <string>
#include <vector>
#include <map>

namespace db {
    using RawRow = char**;

    struct ResultSet {
        MYSQL_RES* res;
        uint64_t row_count;
        int num_fields;
    };

    // 插入或更新时使用的键值映射
    using FieldMap = std::map<std::string, std::string>;

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
        std::vector<RawRow> execute_query(const std::string &sql);

        /**
         * @brief 异步更新：由协程调用
         */
        int execute_update(const std::string &sql);

        void free_result();

    private:
        void wait_io(int status);

        std::vector<RawRow> parse_result(MYSQL_RES *res);

        MYSQL *mysql_;
    };
} // namespace db
