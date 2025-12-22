#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include "../core/mysql_pool.h"
#include "model.h"
#include "../../../include/runtime/goroutine.h"

namespace db {
    template<typename T>
    class QueryBuilder {
    public:
        explicit QueryBuilder(std::string table) : table_name_(std::move(table)) {
        }

        /**
         * @brief 链式条件查询
         * @example .where("age", ">", "18")
         */
        QueryBuilder &where(const std::string &column, const std::string &op, const std::string &value) {
            if (!where_clause_.empty()) {
                where_clause_ += " AND ";
            }
            // 简单处理 SQL 注入：给值加上单引号
            where_clause_ += column + " " + op + " '" + value + "'";
            return *this;
        }

        /**
         * @brief 执行查询并返回模型集合
         */
        std::vector<T> model() {
            std::string sql = build_select_sql();

            // 1. 获取数据库连接驱动
            MySQLDriver *drv = get_driver_safely();

            // 2. 执行查询
            auto result = drv->execute_query(sql);

            // 3. 归还连接
            MySQLPool::get().release(drv);

            // 4. 将原始结果映射为对象
            std::vector<T> results;
            results.reserve(result.rows.size());

            for (auto &row : result.rows) {
                T obj;
                obj.from_row(row); // 此时 result.res 还没析构，指针绝对安全
                results.push_back(std::move(obj));
            }
            return results;
        }

        /**
         * @brief 插入模型数据
         */
        bool insert(const T &obj) {
            // 1. 注意这里：获取的是 FieldMap (Map类型)，而不是 RawRow (指针类型)
            FieldMap data = obj.to_row();
            std::string sql = build_insert_sql(data);

            MySQLDriver *drv = get_driver_safely();
            int affected = drv->execute_update(sql);
            MySQLPool::get().release(drv);

            return affected > 0;
        }

        // 可以继续扩展 update, delete...

    private:
        /**
         * @brief 核心逻辑：如果池中无连接，挂起当前协程，直到获得连接
         */
        MySQLDriver *get_driver_safely() {
            MySQLDriver *drv = MySQLPool::get().acquire();
            assert(drv != nullptr);
            return drv;
        }

        std::string build_select_sql() {
            std::stringstream ss;
            ss << "SELECT * FROM " << table_name_;
            if (!where_clause_.empty()) {
                ss << " WHERE " << where_clause_;
            }
            ss << ";";
            return ss.str();
        }

        std::string build_insert_sql(const FieldMap &data) {
            if (data.empty()) return "";

            std::stringstream cols, vals;
            for (auto it = data.begin(); it != data.end(); ++it) {
                cols << it->first << (std::next(it) == data.end() ? "" : ", ");
                vals << "'" << it->second << "'" << (std::next(it) == data.end() ? "" : ", ");
            }
            std::stringstream ss;
            ss << "INSERT INTO " << table_name_ << " (" << cols.str() << ") VALUES (" << vals.str() << ");";
            return ss.str();
        }

        std::string table_name_;
        std::string where_clause_;
    };

    /**
     * @brief 全局辅助函数，开启查询链
     * @example db::table<User>("users").where(...).model();
     */
    template<typename T>
    QueryBuilder<T> table(std::string name) {
        return QueryBuilder<T>(std::move(name));
    }
} // namespace db
