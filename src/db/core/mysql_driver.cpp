#include "mysql_driver.h"
#include "../../../include/runtime/goroutine.h"
#include "../../../include/runtime/netpoller.h"
#include <iostream>


namespace db {
    MySQLDriver::MySQLDriver() : mysql_(nullptr) {
    }

    MySQLDriver::~MySQLDriver() {
        if (mysql_) mysql_close(mysql_);
    }

    /**
     * 主线程同步连接逻辑
     */
    bool MySQLDriver::connect_sync(const std::string &host, const std::string &user,
                                   const std::string &pass, const std::string &dbname) {
        mysql_ = mysql_init(nullptr);
        if (!mysql_) return false;

        // 1. 先进行标准的阻塞连接
        auto ret = mysql_real_connect(mysql_, host.c_str(), user.c_str(),
                                      pass.c_str(), dbname.c_str(), 3306, nullptr, 0);

        if (ret) {
            // 2. 连接成功后，立即开启非阻塞模式
            // 这样后续的 query/store_result 就可以通过异步 start/cont 函数集来操作
            mysql_options(mysql_, MYSQL_OPT_NONBLOCK, nullptr);
            return true;
        }
        return false;
    }

    void MySQLDriver::wait_io(int status) {
        int fd = mysql_get_socket(mysql_);
        auto current_g = runtime::Goroutine::current();

        // 只有在协程中调用才需要挂起
        if (current_g) {
            if (status & MYSQL_WAIT_READ) {
                runtime::Netpoller::get().watch(fd, runtime::IOEvent::Read, current_g);
            } else if (status & MYSQL_WAIT_WRITE) {
                runtime::Netpoller::get().watch(fd, runtime::IOEvent::Write, current_g);
            }
            runtime::Goroutine::yield();
        }
    }

    std::vector<RowData> MySQLDriver::execute_query(const std::string &sql) {
        int status, err;

        // 所有的 query 操作依然使用异步版本，这样能保证多协程并发时不阻塞 Worker 线程
        status = mysql_real_query_start(&err, mysql_, sql.c_str(), sql.length());
        while (status != 0) {
            wait_io(status);
            status = mysql_real_query_cont(&err, mysql_, status);
        }

        MYSQL_RES *res = nullptr;
        status = mysql_store_result_start(&res, mysql_);
        while (status != 0) {
            wait_io(status);
            status = mysql_store_result_cont(&res, mysql_, status);
        }

        return parse_result(res);
    }

    int MySQLDriver::execute_update(const std::string &sql) {
        int status, err;
        status = mysql_real_query_start(&err, mysql_, sql.c_str(), sql.length());
        while (status != 0) {
            wait_io(status);
            status = mysql_real_query_cont(&err, mysql_, status);
        }
        return (err == 0) ? (int) mysql_affected_rows(mysql_) : -1;
    }

    std::vector<RowData> MySQLDriver::parse_result(MYSQL_RES *res) {
        std::vector<RowData> results;
        if (!res) return results;

        int num_fields = mysql_num_fields(res);
        MYSQL_FIELD *fields = mysql_fetch_fields(res);
        MYSQL_ROW row;

        while ((row = mysql_fetch_row(res))) {
            RowData row_map;
            for (int i = 0; i < num_fields; i++) {
                row_map[fields[i].name] = row[i] ? row[i] : "NULL";
            }
            results.push_back(row_map);
        }
        mysql_free_result(res);
        return results;
    }
} // namespace db
