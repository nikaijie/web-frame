cmake_minimum_required(VERSION 3.20)
project(webFrame)

set(CMAKE_CXX_STANDARD 17)

# 1. 寻找 Boost 库 (必须包含 context 组件)
find_package(Boost REQUIRED COMPONENTS context)

# 2. 定义可执行文件
# 注意：头文件 (.h) 其实不需要放在 add_executable 里，
# 但放进去可以让 CLion 等 IDE 的索引更智能。
add_executable(webFrame
        main.cpp
        src/web/gee.cpp
        src/web/request.cpp
        src/web/response.cpp
        src/runtime/goroutine.cpp
        include/runtime/goroutine.h
        include/runtime/scheduler.h
        src/runtime/scheduler.cpp)

# 3. 指定包含路径
# PRIVATE 表示这些头文件只用于编译此 target
target_include_directories(webFrame PRIVATE
        include
        ${Boost_INCLUDE_DIRS})

# 4. 关键：链接 Boost 库 (这是解决 Symbol 报错的核心)
target_link_libraries(webFrame PRIVATE Boost::context)