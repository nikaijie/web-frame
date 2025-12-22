#pragma once
#include <mysql.h>
#include <string>
#include <vector>
#include <map>

namespace db {
    using RawRow = char **;


    struct QueryResult {
        MYSQL_RES *res; // 真正的内存持有者
        std::vector<RawRow> rows; // 快速访问的指针数组

        QueryResult(MYSQL_RES *r, std::vector<RawRow> &&rv)
            : res(r), rows(std::move(rv)) {
        }


        ~QueryResult() {
            if (res) {
                mysql_free_result(res); // 对象析构时，自动释放 20GB 内存的源头
            }
        }

        // 禁止拷贝，防止双重释放，但支持移动
        QueryResult(const QueryResult &) = delete;

        QueryResult &operator=(const QueryResult &) = delete;

        QueryResult(QueryResult &&other) noexcept
            : res(other.res), rows(std::move(other.rows)) {
            other.res = nullptr;
        }
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
        QueryResult execute_query(const std::string &sql);

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
