webIO/
├── include/                      # 公共头文件目录
│   ├── web/                      # Web框架相关头文件
│   │   └── gee.h                 # Gee Web框架主类定义
│   └── runtime/                  # 运行时相关头文件
│       └── spinlock.h            # 自旋锁定义
├── src/                          # 源代码目录
│   ├── db/                       # 数据库相关代码
│   │   ├── db.h                  # 数据库初始化接口
│   │   ├── core/                 # 数据库核心实现
│   │   │   ├── mysql_driver.cpp  # MySQL驱动实现
│   │   │   ├── mysql_driver.h    # MySQL驱动接口
│   │   │   ├── mysql_pool.cpp    # MySQL连接池实现
│   │   │   └── mysql_pool.h      # MySQL连接池接口
│   │   └── orm/                  # ORM相关代码
│   │       ├── model.h           # ORM模型基类
│   │       └── query_builder.h   # SQL查询构建器
│   ├── runtime/                  # 运行时相关实现
│   │   ├── goroutine.cpp         # 协程实现
│   │   ├── netpoller.cpp         # 网络轮询器实现
│   │   └── scheduler.cpp         # 调度器实现
│   ├── test/                     # 测试相关代码
│   │   └── mysql.cpp             # MySQL测试逻辑
│   └── web/                      # Web框架实现
│       ├── gee.cpp               # Gee Web框架实现
│       ├── request.cpp           # 请求处理实现
│       └── response.cpp          # 响应处理实现
├── .idea/                        # IDEA项目配置目录
│   ├── vcs.xml                   # 版本控制配置
│   ├── modules.xml               # 项目模块配置
│   └── workspace.xml             # 工作区配置
└── .git/                         # Git版本控制目录