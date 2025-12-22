#pragma once
#include "runtime/goroutine.h"

namespace runtime {
    enum class IOType { DATABASE, WEB };
    struct IOContextBase {
        int fd;
        IOType type;
        Goroutine::Ptr waiting_g; // 核心：不管是 DB 还是 Web，都需要唤醒协程

        virtual ~IOContextBase() = default;
        IOContextBase(int f, IOType t) : fd(f), type(t) {}
    };
}