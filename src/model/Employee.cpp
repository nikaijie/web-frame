#include <string>
#include "../db/db.h"

struct Employee : public db::Model {
    int id;
    std::string name;
    std::string department;
    int salary;

    std::string table_name() const override {
        return "employees";
    }


    // 性能核心：通过 char** 原始指针直接赋值，避免 Map 开销
    void from_row(db::RawRow row) override {
        // row[i] 直接指向驱动内部缓冲区
        id = row[0] ? std::atoi(row[0]) : 0;
        name = row[1] ? row[1] : "";
        department = row[2] ? row[2] : "";
        salary = row[3] ? std::atoi(row[3]) : 0;
    }


    // 用于写入操作的映射
    db::FieldMap to_row() const override {
        return {
            {"id", std::to_string(id)},
            {"name", name},
            {"department", department},
            {"salary", std::to_string(salary)}
        };
    }

    void write_json(std::string &out) const {
        out.append("{\"id\":").append(std::to_string(id))
                .append(",\"name\":\"").append(name)
                .append("\",\"department\":\"").append(department)
                .append("\",\"salary\":").append(std::to_string(salary))
                .append("}");
    }
};
