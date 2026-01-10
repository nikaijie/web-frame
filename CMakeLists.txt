cmake_minimum_required(VERSION 3.20)
project(webFrame)

# 消除 Boost 寻找策略的警告
cmake_policy(SET CMP0167 OLD)

set(CMAKE_CXX_STANDARD 17)

# 1. 寻找 Boost 库
find_package(Boost REQUIRED COMPONENTS context)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

# 2. 指定 MariaDB 路径 (请确保已执行 brew install mariadb-connector-c)
set(MARIADB_PATH "/opt/homebrew/opt/mariadb-connector-c")

# 3. 定义源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
list(APPEND SOURCES "main.cpp")

# 4. 创建可执行文件 (必须先定义这个 target)
add_executable(webFrame ${SOURCES}
        src/db/core/mysql_driver.cpp
        src/db/core/mysql_driver.h
        src/db/core/mysql_pool.cpp
        src/db/core/mysql_pool.h
        src/db/orm/model.h
        src/db/orm/query_builder.h
        src/db/db.h
        include/runtime/spinlock.h
        src/test/mysql.h
        src/web/core/gee.cpp
        include/web/core/gee.h
        include/runtime/context/iocontext.h
        include/runtime/context/db_context.h
        include/runtime/context/web_context.h
        src/util/logger.h
        src/runtime/context/web_context.cpp
        src/test/web.h
        src/util/logger.h
        include/web/core/node.h
        src/web/core/router.cpp
        src/model/Employee.cpp
        src/web/protocol/request.cpp
        include/web/protocol/MultipartProcessor.h
        include/pool/io_task_pool.h
        include/data_structure/co_mutex.h
        src/data_structure/co_mutex.cpp
        include/data_structure/channel.h
        include/data_structure/wait_group.h
        src/data_structure/wait_group.cpp
        include/data_structure/context.h
        src/data_structure/context.cpp
)

# 4. 指定包含路径 (MariaDB 的头文件结构略有不同)
target_include_directories(webFrame PRIVATE
        include
        ${Boost_INCLUDE_DIRS}
        ${MARIADB_PATH}/include
        ${MARIADB_PATH}/include/mariadb # 关键：MariaDB 的 mysql.h 通常在这里
)

target_link_libraries(webFrame PRIVATE
        Boost::context
        spdlog::spdlog
        fmt::fmt
        "${MARIADB_PATH}/lib/mariadb/libmariadb.dylib"
)

# 7. 添加宏定义
target_compile_definitions(webFrame PRIVATE MARIADB_NONBLOCKING=1)