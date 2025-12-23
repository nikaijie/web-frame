#pragma once
#include <string>
#include <vector>
#include "../core/mysql_driver.h"

namespace db {

    class Model {
    public:
        virtual ~Model() = default;

        // --- 数据库操作接口 ---
        virtual void from_row(RawRow row) = 0;
        virtual FieldMap to_row() const = 0;
        virtual std::string table_name() const = 0;

        // 序列化接口
        virtual void write_json(std::string &out) const = 0;
    };

} // namespace db