#pragma once
#include <string>
#include <vector>
#include "../core/mysql_driver.h"

namespace db {

    class Model {
    public:
        virtual ~Model() = default;

        // 这里的参数类型必须是 RawRow (也就是 char**)
        virtual void from_row(RawRow row) = 0;

        // 这里的返回类型必须是 FieldMap (也就是 std::map<std::string, std::string>)
        virtual FieldMap to_row() const = 0;

        virtual std::string table_name() const = 0;
    };

} // namespace db