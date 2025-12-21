#include <mysql.h>
#include "runtime/netpoller.h"
#include "runtime/goroutine.h"
#include <iostream>
#include <vector>

namespace db {

class MySQLCoro {
public:

    MYSQL* connect(const char* host, const char* user, const char* pass, const char* db_name, bool async = true) {
        MYSQL* mysql = mysql_init(nullptr);

        if (async) {
            mysql_options(mysql, MYSQL_OPT_NONBLOCK, nullptr);
            MYSQL* ret;
            int status = mysql_real_connect_start(&ret, mysql, host, user, pass, db_name, 3306, nullptr, 0);
            while (status != 0) {
                wait_io(mysql, status);
                status = mysql_real_connect_cont(&ret, mysql, status);
            }
        } else {
            // 主线程初始化使用同步阻塞连接
            if (!mysql_real_connect(mysql, host, user, pass, db_name, 3306, nullptr, 0)) {
                fprintf(stderr, "Failed to connect to database: %s\n", mysql_error(mysql));
                return nullptr;
            }
            // 连接成功后，再开启非阻塞模式供后续协程使用
            mysql_options(mysql, MYSQL_OPT_NONBLOCK, nullptr);
        }
        return mysql;
    }

    // 核心异步查询
    std::string query(MYSQL* mysql, const std::string& sql) {
        int status;
        int ret_err;

        status = mysql_real_query_start(&ret_err, mysql, sql.c_str(), sql.length());
        while (status != 0) {
            wait_io(mysql, status);
            status = mysql_real_query_cont(&ret_err, mysql, status);
        }

        MYSQL_RES* res = nullptr;
        status = mysql_store_result_start(&res, mysql);
        while (status != 0) {
            wait_io(mysql, status);
            status = mysql_store_result_cont(&res, mysql, status);
        }

        return process_result(res);
    }

private:
    void wait_io(MYSQL* mysql, int status) {
        int fd = mysql_get_socket(mysql);
        auto current_g = runtime::Goroutine::current();

        if (status & MYSQL_WAIT_READ) {
            runtime::Netpoller::get().watch(fd, runtime::IOEvent::Read, current_g);
        } else if (status & MYSQL_WAIT_WRITE) {
            runtime::Netpoller::get().watch(fd, runtime::IOEvent::Write, current_g);
        }
        runtime::Goroutine::yield();
    }

    std::string process_result(MYSQL_RES* res) {
        if (!res) return "OK";
        MYSQL_ROW row = mysql_fetch_row(res);
        std::string val = row ? row[0] : "EOF";
        mysql_free_result(res);
        return val;
    }
};

}