#pragma once
#include <string>

#include "iocontext.h"
#include "runtime/goroutine.h"

namespace runtime {
    struct DBContext : public runtime::IOContextBase {
        std::string read_buf;
        std::string write_buf;
        size_t expected_size = 0;
        DBContext(int f) : IOContextBase(f, runtime::IOType::DATABASE) {
        }
    };
}
