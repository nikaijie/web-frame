#pragma once
#include <string>
#include <map>
#include <vector>

namespace db {
    /**
     * @brief RowData 是底层驱动返回的原始数据行
     * Key: 列名, Value: 字符串格式的数据
     */
    using RowData = std::map<std::string, std::string>;

    /**
     * @brief Model 基类
     * 所有想要使用 ORM 功能的实体类都必须继承此类
     */
    class Model {
    public:
        virtual ~Model() = default;

        /**
         * @brief 由子类实现：定义如何从数据库行数据填充对象成员
         * @example name = data.at("name"); age = std::stoi(data.at("age"));
         */
        virtual void from_row(const RowData &data) = 0;

        /**
         * @brief 由子类实现：将对象成员转换为数据库可识别的 Map
         * 用于 Insert 和 Update 操作
         */
        virtual RowData to_row() const = 0;

        /**
         * @brief 子类需提供对应的数据库表名
         */
        virtual std::string table_name() const = 0;
    };
} // namespace db
