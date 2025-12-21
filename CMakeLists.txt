cmake_minimum_required(VERSION 3.20)
project(webFrame)

# 消除 Boost 寻找策略的警告
cmake_policy(SET CMP0167 OLD)

set(CMAKE_CXX_STANDARD 17)

# 1. 寻找 Boost 库
find_package(Boost REQUIRED COMPONENTS context)

# 2. 指定 MariaDB 路径 (请确保已执行 brew install mariadb-connector-c)
set(MARIADB_PATH "/opt/homebrew/opt/mariadb-connector-c")

# 3. 定义源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")
list(APPEND SOURCES "main.cpp")

add_executable(webFrame ${SOURCES}
        src/db/core/mysql_driver.cpp
        src/db/core/mysql_driver.h
        src/db/core/mysql_pool.cpp
        src/db/core/mysql_pool.h
        src/db/orm/model.h
        src/db/orm/query_builder.h
        src/db/db.h)

# 4. 指定包含路径 (MariaDB 的头文件结构略有不同)
target_include_directories(webFrame PRIVATE
        include
        ${Boost_INCLUDE_DIRS}
        ${MARIADB_PATH}/include
        ${MARIADB_PATH}/include/mariadb # 关键：MariaDB 的 mysql.h 通常在这里
)

# 5. 链接库 (直接链接 dylib 文件最稳妥，避免路径歧义)
target_link_libraries(webFrame PRIVATE
        Boost::context
        "${MARIADB_PATH}/lib/mariadb/libmariadb.dylib"
)

# 6. 添加异步编译宏
add_definitions(-DMARIADB_NONBLOCKING=1)