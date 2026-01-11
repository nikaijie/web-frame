#include "../include/data_structure/context.h"
#include "../include/runtime/scheduler.h"

namespace runtime {

    Context::Ptr Context::WithTimeout(int timeout_ms) {
        auto ctx = std::make_shared<Context>();

        //
        runtime::go([ctx, timeout_ms]() {
            runtime::sleep(timeout_ms);
            ctx->cancel();
        });

        return ctx;
    }

}