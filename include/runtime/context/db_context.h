#pragma once
#include <string>

#include "iocontext.h"
#include "runtime/goroutine.h"

namespace runtime {
    struct DBContext : public runtime::IOContextBase {
        DBContext(int f) : IOContextBase(f, runtime::IOType::DATABASE) {
        }
    };
}
