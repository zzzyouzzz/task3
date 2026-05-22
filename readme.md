# 物流管理系统 (Logistics Management System)

## 📋 项目概述

一个基于 **C++17** 的客户端-服务器（C/S）架构快递物流管理系统，提供完整的物流业务生命周期管理。系统采用 **select 模型 + 非阻塞 socket** 实现单线程多客户端并发通信，结合分层架构设计，兼顾性能表现与代码可维护性。

---

## 🏗️ 系统架构

### 三层架构

系统遵循经典的三层架构设计：

```
┌───────────────────────────────────────────────┐
│              表示层 (Presentation)              │
│    CLI（命令行界面）│    Qt GUI（图形界面）       │
├───────────────────────────────────────────────┤
│              控制层 (Controller)                │
│    C/S 网络通信 · 协议编解码 · 会话管理        │
│    请求校验 · 权限验证 · 请求分发              │
├───────────────────────────────────────────────┤
│              业务&数据层 (Service + Data)       │
│    LogisticsSystem                              │
│    ├─ 内存数据: m_users, m_parcels              │
│    └─ 业务逻辑: 用户管理·快递管理·计费·统计    │
├───────────────────────────────────────────────┤
│              持久化层 (Persistence)              │
│    FileManager · 文件序列化/反序列化            │
└───────────────────────────────────────────────┘
```

### 架构说明

| 层级 | 职责 | 关键类/模块 |
|------|------|-------------|
| **表示层** | 用户交互界面，菜单驱动操作 | `LogisticsClient` (CLI), Qt GUI |
| **控制层** | C/S 通信、请求校验、权限验证、请求分发 | `Server`, `Communication`, `Protocol` |
| **业务&数据层** | **持有全部数据**（全量内存加载）+ **业务逻辑** | `LogisticsSystem` |
| **持久化层** | 仅负责文件 I/O，不持有数据 | `FileManager` |

> **关键设计决策**：`LogisticsSystem` 的成员变量 `m_users` / `m_parcels` 即为**数据在内存中的本体**。系统启动时通过 `FileManager::loadUsers()` / `loadParcels()` 一次性全量加载到内存，运行中的所有操作直接读写内存中的 `std::map`，关闭时通过 `FileManager` 全量写回文件。    
> `FileManager` **不是**数据存储层——它不保存任何状态，仅提供文件序列化/反序列化的工具方法。
>
> **为何不抽独立的数据层？** —— 当前项目数据量完全可全量装入内存，强行引入 Repository 模式只会制造大量透传函数（如 `queryParcels()` 直接委托 `find()`），带来冗余而无实际收益。若未来数据量膨胀到内存无法容纳，架构自然演化为：
>
> ```
> 服务层 (Service)
>    LogisticsSystem
>    - 不再持有 m_users / m_parcels
>    - 通过数据层接口查询/修改
>    - 专注业务规则: 计费/状态机/权限
> ═══════════════════════════════════
> 数据层 (Repository)
>    UserRepository / ParcelRepo
>    - 封装底层存储（SQL/文件/缓存）
>    - 提供 find() / save() / delete()
>    - 只返回请求的数据，不缓存全量
> ```
>
> 原则很简单：
> | 场景 | 设计模式 | 本层职责 |
> |------|---------|---------|
> | **全量内存装得下** | 当前项目 | `LogisticsSystem` 同时持有数据 + 执行业务，`FileManager` 仅作序列化工具 |
> | **全量内存装不下** | Repository 模式 | 服务层依赖数据层接口，各自职责真正分离，底层存储可切换（SQL/文件/缓存） |

> **关键设计决策：权限校验放在控制层**：
> ```
> 控制层 (Server::processCommand)
>     ↓ 权限校验 → 拦截非法请求，不碰服务层
>     ↓ 参数校验
>     ↓ 调用
> 服务层 (LogisticsSystem)        ← 纯业务，无权限概念
>     ↓ 操作数据
> ```
> 
> 本系统的权限校验全部集中在 `Server::processCommand()` 中，即在**调用服务层之前**拦截非法请求。`LogisticsSystem`（业务&数据层）中没有权限判断代码——它只关心业务计算，不关心调用者是谁。
> 
> 这样做的好处：
> - **服务层可测试性更强**：测试 `sendParcel()` 无需构造不同角色用户，直接传参数测业务逻辑即可
> - **控制层与服务层职责清晰**：权限是"能不能调用"的问题，不是业务逻辑本身
> - **多客户端支持**：若未来增加 Web 管理后台等新客户端，权限策略可能不同，修改控制层即可，服务层无需改动
> 
> 客户端 CLI 中也做了角色判断（如仅管理员才显示"分配快递员"菜单项），但那属于 **UI 交互逻辑**，目的只是提升用户体验、减少不必要的网络往返。真正的安全边界仍在服务端控制层。

---

## 🚀 快速开始

### 环境要求

- CMake ≥ 3.16
- 支持 C++17 的编译器（GCC 8+, MSVC 2019+, Clang 10+）
- Qt 6（仅构建图形界面时可选）
- Windows: WinSock2（系统自带）
- Linux: POSIX socket 库（系统自带）

### 构建

```bash
# 完整构建（含 CLI、Server、Tests）
mkdir build && cd build
cmake ..
cmake --build .

# 构建所有目标（含 Qt GUI）
cmake .. -DBUILD_QT_GUI=ON
cmake --build .

# 运行测试
ctest
```

### 构建产物

构建完成后，可执行文件输出至 `output/` 目录：

```
output/
├── client/
│   ├── client_cli.exe          # 命令行客户端
│   └── client_qt.exe           # Qt 图形界面客户端（可选）
├── server/
│   └── server.exe              # 服务器程序
└── tests/
    ├── CommonTests.exe         # 公共模块单元测试（协议、日志）
    ├── DataSetTests.exe        # 数据集加载完整性测试
    └── IntegrationTest.exe     # 全流程自动化集成测试（脚本解释器模式）
```

---

## ⚙️ 配置说明

### 配置文件路径

服务器与客户端启动时自动读取同级目录下的配置文件：

- 服务器: `output/server/server_config.txt`
- 客户端: `output/client/client_config.txt`

### 服务器配置 (`server_config.txt`)

```ini
# 监听 IP 地址（0.0.0.0 监听所有网卡）
ip = 0.0.0.0
# 监听端口
port = 8888
# 日志级别: DEBUG / INFO / WARNING / ERROR / FATAL
log_level = INFO
# 数据文件路径
user_file = users.dat
parcel_file = parcels.dat
config_file = config.dat
# 是否自动分配快递员: true / false
auto_assign_courier = false
# 日志输出方式: console / file / both
log_output = both
# 最大空闲超时(秒): 客户端超过此时间无活动即断开连接
max_idle_time = 180
```

### 客户端配置 (`client_config.txt`)

```ini
ip = 127.0.0.1
port = 8888
```

---

## � 数据模型

系统管理两类核心数据：**用户** 与 **包裹**，均以文本文件持久化存储。

### 用户表

用户信息通过 `User` 类体系（基类 + 多态派生）存储，持久化文件每行一条记录，`|` 分隔字段：

```
类型|用户名|密码|真实姓名|电话|地址|余额
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `类型` | `int` | 0=客户, 1=快递员, 2=管理员 |
| `用户名` | `string` | 全局唯一标识 |
| `密码` | `string` | 当前明文存储 |
| `真实姓名` | `string` | 用户真实姓名 |
| `电话` | `string` | 联系电话 |
| `地址` | `string` | 联系地址 |
| `余额` | `double` | 账户余额（管理员账户余额无意义，使用公司资金池） |

**文件**: `users.dat`

### 包裹表

包裹信息通过 `Parcel` 类体系（基类 + `NormalParcel`/`FragileParcel`/`BookParcel` 派生）存储：

```
包裹类型|单号|寄件人|收件人|寄送时间|签收时间|状态|描述|重量|快递员
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `包裹类型` | `int` | 0=普通, 1=易碎, 2=书籍 |
| `单号` | `string` | 格式 `PCL{时间戳}-{递增序号}`，全局唯一 |
| `寄件人` | `string` | 寄件客户用户名 |
| `收件人` | `string` | 收件客户用户名 |
| `寄送时间` | `time_t` | Unix 时间戳，创建时自动生成 |
| `签收时间` | `time_t` | 签收时记录，0 表示未签收 |
| `状态` | `int` | 0=待揽收, 1=待签收, 2=已签收 |
| `描述` | `string` | 物品描述 |
| `重量` | `double` | 普通/易碎包裹表示 kg，书籍表示数量 |
| `快递员` | `string` | 分配的快递员用户名，空表示未分配 |

**文件**: `parcels.dat`

### 配置表

仅存储管理员总余额（公司资金池）：

```
金额
```

**文件**: `config.dat`

### 文件格式：版本头

所有数据文件首行为版本头 `V=版本号`（整数递增），用于事务一致性校验：

```
V=1
<数据行 1>
<数据行 2>
...
```

启动时 `LogisticsSystem` 比较三个数据文件（users.dat / parcels.dat / config.dat）的版本号，若不匹配则记录警告。每次 `saveData()` 将版本号 +1 后同时写入三个文件，保证跨文件事务一致性。写操作采用**临时文件写入 + `rename()` 原子重命名**策略，防止写操作中途崩溃导致文件损坏。

### 数据生命周期

```
服务启动
    ↓
FileManager::loadUsers()  / loadParcels() / loadConfig()
    ↓
全量加载到 LogisticsSystem::m_users / m_parcels（内存中的 std::map）
    ↓
服务运行 — 所有操作直接读写内存
    ↓
每次写操作 → FileManager::saveUsers() / saveParcels() / saveConfig() 全量写回文件
    ↓
服务关闭（析构函数）→ saveData() → 释放所有动态内存
```

> **注意**: 每次 `saveData()` 都是 **全量写回而非增量追加**，以保证文件数据与内存状态一致且无冗余记录。

---

## �🔧 系统功能

### 用户角色与权限

| 功能 | 客户 (CUSTOMER) | 快递员 (COURIER) | 管理员 (ADMINISTRATOR) |
|------|:---:|:---:|:---:|
| 登录/注册 | ✅ | ✅ | ✅（内置） |
| 发送快递 | ✅ | ❌ | ❌ |
| 签收快递 | ✅ | ❌ | ❌ |
| 查询快递 | ✅（仅本人相关） | ✅（仅分配到的） | ✅（全部） |
| 充值余额 | ✅ | ❌ | ❌ |
| 查询余额 | ✅ | ✅ | ✅（公司资金池） |
| 修改密码 | ✅ | ✅ | ✅ |
| 分配快递员 | ❌ | ❌ | ✅ |
| 揽收快递 | ❌ | ✅ | ❌ |
| 查询用户 | ❌ | ❌ | ✅ |
| 注销账户 | ❌ | ❌ | ✅ |
| 删除快递 | ❌ | ❌ | ✅ |
| 统计信息 | ❌ | ❌ | ✅ |

### 业务功能

1. **用户注册** — 任何人可注册为客户或快递员，用户名唯一
2. **用户登录/注销** — 校验用户名、密码、角色三重匹配；登录去重（禁止重复登录）
3. **发送快递** — 客户填写收件人、类型（普通/易碎/书籍）、重量/数量、物品描述，系统自动扣费并生成唯一单号（格式 `PCL{时间戳}-{递增序号}`）
4. **快递分配** — 管理员将待揽收快递分配给指定快递员；支持**自动分配策略**：系统选择当前负载最少的快递员
5. **快递揽收** — 快递员批量揽收，从公司资金池结算 50% 运费作为佣金入快递员个人余额
6. **快递签收** — 收件客户批量签收，记录签收时间
7. **快递查询** — 多条件组合查询（单号/寄件人/收件人/快递员/状态/时间范围），不同角色可见范围不同
8. **用户管理** — 管理员查询/删除用户（仅可删除已完成全部业务的用户，不可删除管理员自身）
9. **余额管理** — 充值、查询余额（管理员查询公司资金池）、密码修改
10. **快递删除** — 管理员删除已签收的快递记录
11. **统计报表** — 管理员查看全局统计数据（总用户数/总包裹数/待揽收/已揽收/已签收/资金池余额）

---

## 💻 网络通信协议

### 通信模型

- **模式**: C/S 架构，TCP 长连接
- **模型**: select + 非阻塞 socket，单线程多客户端
- **端口**: 默认 8888

### 协议格式

采用 **`|` 分隔的文本协议**，以 `\n` 作为报文结束标志：

```text
# 请求格式
命令|参数1|参数2|...\n

# 响应格式
RESPONSE|状态码|数据1|数据2|...\n

# 示例：登录请求
LOGIN|alice|mypassword|0\n

# 示例：登录成功响应（状态码 0 = SUCCESS）
RESPONSE|0|alice|0\n

# 示例：登录失败响应（状态码 9 = LOGIN_FAILED）
RESPONSE|9|Login failed\n
```

> 状态码采用数字枚举（`ErrorCode`），0 表示成功，非 0 表示对应的错误类型。

### 半包处理机制

由于 TCP 是流式协议，可能导致：
1. **半包（TCP 分片）** — 一个完整的应用层报文被拆分成多个 TCP 包到达
2. **粘包（多包合并）** — 多个应用层报文在一个 TCP 包中到达

服务端采用**累积缓冲区策略**处理：

```
[建立连接] → [创建 ClientHandler]
     ↓
[recv() 读取数据] → [追加到 buf 缓冲区]
     ↓
[搜索 \n] → 未找到 → [返回 select 循环，继续等待数据]
     ↓ 找到
[提取完整报文] → [从 buf 中移除] → [解析 + 处理] → [发送响应]
     ↓
[当前 buf 中可能还有更多报文，循环处理直到耗尽]
     ↓
[返回 select 循环，服务其他客户端]
```

此设计确保：
- 无论 TCP 如何分包/粘包，应用层报文完整性不受影响
- 单次 `recv()` 可包含 0 个、1 个或多个完整报文
- 非阻塞模式下不会阻塞其他客户端

> **不对称设计分析**：在当前的同步请求-响应模式下，服务端对客户端请求的半包处理几乎不会被触发——客户端请求通常只有几十字节，远小于 TCP 分片阈值，且每次发送一个请求后即等待响应，不存在流水线发送。该缓存机制属于防御性设计，当前场景下英雄无用武之地。
>
> **反观客户端读取服务端响应**：`QUERY_PARCEL` 等查询操作的响应可能包含大量快递的序列化数据，必然被 TCP 分片。客户端 `sendRequest` 通过 **循环 `recv()` + `\n` 边界检测**处理大包接收：自增拼接缓冲区直到收到换行符，确保完整报文到达后再解析。
>
> | 方向 | 数据大小 | 分片风险 | 当前处理 | 正确性 |
> |------|---------|---------|---------|:------:|
> | 客户端 → 服务端（请求） | 小（< 100 字节） | 极低 | 有累积缓冲区 | 防御性设计 |
> | 服务端 → 客户端（响应） | 可能很大 | 高 | 循环 recv + \n 边界检测 | 已完善 |
>
> 服务端保留此设计没有坏处——每个连接仅多一个 `std::string` 的开销。若日后改为异步流水线或多请求复用模式，它自然派有用场。

### 进程上下文设计

服务端为每个客户端连接维护一个 `ClientHandler` 对象（已从最初的 `Client_info` 结构体演化为完整类）：

```cpp
class ClientHandler {
    int requestId;              // 当前请求递增 ID（日志追踪）
    time_t lastActiveTime;      // 最后活跃时间（空闲超时检测）
    std::string m_currentUser;  // 当前登录用户名（空 = 未登录）
    UserType m_userType;        // 当前登录用户类型
    std::string buf;            // 累积接收缓冲区，用于 TCP 半包重组
};
```

设计意义：
- **会话保持**: 记录每个连接的登录状态与用户类型，区分已登录/未登录用户
- **空闲超时**: 记录最后活跃时间，配合 `max_idle_time` 配置自动断开僵尸连接
- **半包缓存**: 解决 TCP 流式传输的分片问题，在收到完整 `\n` 终止报文前暂存数据
- **请求追踪**: 为每个请求分配递增 ID，日志中可追踪从收到到响应的完整链路
- **权限校验**: 根据 `m_userType` 在命令路由时拦截非法请求（权限校验集中在控制层）

服务端通过 `std::map<socket_t, ClientHandler>` 管理所有连接，每个 socket 对应一个 `ClientHandler`——在单线程 select 模型中，各连接的状态天然隔离，不会相互干扰。

> **与协程思想的呼应**：`select` + per-connection `ClientHandler` 在思路上与协程（coroutine）有异曲同工之处——它们都基于**合作式多任务（cooperative multitasking）**：
>
> | | select + ClientHandler | 协程 |
> |---|---|:---:|
> | **并发单位** | 每个连接一个 `ClientHandler` | 每个 `co_await` 一个协程帧 |
> | **挂起点** | 函数返回 → 回到 select 循环 | `co_await` → 挂起当前协程 |
> | **恢复触发** | select 检测到 socket 可读 | I/O 完成 / 定时器到期 |
> | **状态保存** | 手动管理：`ClientHandler` 成员变量 | 自动管理：堆上分配的协程帧 |
> | **调度器** | 手写的 select 主循环 | 运行时内置的 executor |
>
> 可以把当前的实现看作一个**「手动版协程」**——协程由运行时自动替你保存局部变量和执行位置，这里则通过 `ClientHandler` 手动保存每个连接的状态，由 select 循环自行调度。如果将来改用 C++20 coroutine，可以用同步的方式写异步代码，不再需要手动管理 `buf` 的累积状态。

---

## 📁 项目结构

```
task3/
├── CMakeLists.txt                  # 顶层 CMake 构建配置
├── Common/                         # 公共基础库（静态库）
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── common.h                # 协议常量、错误码、安全解析工具
│   │   ├── User.h                  # 用户类体系（基类 + 派生：Customer/Courier/Administrator）
│   │   ├── Parcel.h                # 快递类体系（基类 + 派生：NormalParcel/FragileParcel/BookParcel）
│   │   └── Logger.h                # 日志系统（5 级别 + 5MB 自动轮转）
│   └── src/
│       ├── common.cpp              # Protocol 实现（buildRequest / parseRequest）
│       └── Logger.cpp              # 日志系统完整实现
├── server/                         # 服务端（可执行程序）
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── server.h                # 服务器主类（select 事件循环）
│   │   ├── ClientHandler.h         # 每连接上下文（登录状态/累积缓冲区/请求处理/权限校验）
│   │   ├── LogisticsSystem.h       # 业务逻辑核心类（持有全量数据 + 业务方法）
│   │   └── FileManager.h           # 文件持久化管理类（版本头 + 原子重命名）
│   └── src/
│       ├── main.cpp                # 程序入口 + 信号处理（Ctrl+C 优雅关闭）
│       ├── server.cpp              # 网络通信（select + 非阻塞 socket）
│       ├── ClientHandler.cpp       # 命令路由 + 权限校验 + handler 实现
│       ├── LogisticsSystem.cpp     # 业务逻辑实现（用户/快递/计费/统计）
│       └── FileManager.cpp         # 文件序列化/反序列化实现
├── client/
│   ├── cli/                        # CLI 命令行客户端
│   │   ├── CMakeLists.txt
│   │   ├── Cli.h                   # 命令行界面类
│   │   ├── Cli.cpp                 # UI 交互实现（菜单/输入/错误处理/自动重连）
│   │   └── main.cpp                # 客户端入口
│   ├── qt_ui/                      # Qt 图形界面客户端
│   │   ├── CMakeLists.txt
│   │   ├── include/                # Qt 界面头文件（LoginWindow/UserWindow/CourierWindow 等）
│   │   └── src/                    # Qt 界面实现
│   └── service/                    # 客户端通信服务层（静态库）
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── Communication.h     # 网络通信封装类（含自动重连凭据管理）
│       └── src/
│           └── Communication.cpp   # 通信实现（累积缓冲区 + \n 边界检测 + reconnect）
├── tests/                          # 单元测试与集成测试
│   ├── CMakeLists.txt
│   ├── common_tests.cpp            # 公共模块测试（协议编解码、日志级别、文件写入）
│   ├── dataset_tests.cpp           # 数据集文件加载校验（4 用户 + 3 快递 + 配置）
│   ├── test_runner.h               # 集成测试框架声明
│   ├── test_runner.cpp             # 集成测试脚本解释器（子进程管理/命令解析/断言）
│   ├── full_scenario.in            # 全流程 Happy Path 测试脚本
│   ├── permission_test.in          # 权限验证测试脚本
│   ├── edge_case_test.in           # 异常/边界测试脚本
│   ├── large_packet_test.in        # 大数据包传送测试脚本
│   ├── reconnect_test.in           # 断线重连测试脚本
│   ├── keepalive_test.in           # 连接保活/超时测试脚本
│   └── data/                       # 测试数据（预置用户/包裹/配置）
└── output/                         # 构建输出
    ├── client/
    │   ├── client_config.txt
    ├── server/
    │   ├── server_config.txt
    │   └── server.exe
    └── tests/
        ├── CommonTests.exe
        ├── DataSetTests.exe
        ├── IntegrationTest.exe
        └── ServerTests.exe
```

---

## ⚡ 技术要点

### 单线程多客户端并发

利用 `select()` 系统调用 + 非阻塞 socket 实现单线程同时管理多个客户端连接：

```
主循环:
1. FD_ZERO → 将 listen socket + 所有 client sockets 加入 readfds
2. select(maxFD, &readfds, ...) → 等待可读事件
3. 若 listen socket 可读 → accept() 新连接
4. 遍历 client sockets → 对可读的调用 handleClientRequest()
5. 循环
```

优势：
- 无需锁/同步，无竞态条件
- 无线程上下文切换开销
- 资源占用低（无需每个连接一个线程）

### 快递计费模型

| 快递类型 | 计价规则 | 说明 |
|---------|---------|------|
| **普通包裹** (Normal) | 5.0 × 重量(kg) | 标准费率 |
| **易碎品** (Fragile) | 8.0 × 重量(kg) | 加收易碎附加费 |
| **书籍** (Book) | 2.0 × 数量 | 优惠费率，`weight` 表示数量 |

### 佣金结算

快递员批量揽收快递时，从公司资金池（`m_adminTotalBalance`）中划拨 **50% 运费** 作为揽收佣金，即时入账快递员个人余额。若公司资金池余额不足，该包裹揽收跳过（`continue`）而非失败。

### 日志系统

- 支持 **5 个级别**: DEBUG / INFO / WARNING / ERROR / FATAL
- 可选输出目标: 控制台、文件或同时输出
- **自动日志轮转**: 单文件超过 5MB 自动归档（`.1` → `.2` → `.3`）
- 线程安全: 通过 `std::mutex` 保证并发写入安全
- 请求追踪: 每个请求分配唯一递增 ID，日志中可追踪完整链路

### 数据持久化

- 基于文本文件存储，每行一条记录，首行为版本头 `V=版本号`
- 用户数据: `users.dat`（`|` 分隔字段）
- 快递数据: `parcels.dat`（`|` 分隔字段）
- 配置数据: `config.dat`（仅存管理员总余额）
- **事务保护**：每个数据文件带版本号，`saveData()` 同步递增三文件版本号，启动时校验一致性
- **原子写**：先写入 `.tmp` 临时文件，再通过 `rename()` 原子重命名，防止写崩溃损坏原文件
- 每次写操作后实时落盘（`flush()`），确保数据不丢失

---

## 🧪 测试

```bash
# 运行所有测试
ctest --output-on-failure

# 或直接运行
./output/tests/CommonTests.exe
./output/tests/DataSetTests.exe
./output/tests/IntegrationTest.exe --input=../tests/full_scenario.in --server=../output/server/server.exe
./output/tests/IntegrationTest.exe --input=../tests/large_packet_test.in --server=../output/server/server.exe
./output/tests/IntegrationTest.exe --input=../tests/reconnect_test.in --server=../output/server/server.exe
./output/tests/IntegrationTest.exe --input=../tests/keepalive_test.in --server=../output/server/server.exe
```

### 测试覆盖

| 测试套件 | 覆盖内容 |
|---------|---------|
| **CommonTests** | 协议编解码、日志级别解析、日志文件写入、请求-响应往返测试 |
| **DataSetTests** | 预置数据集的完整加载校验（4 用户 + 3 快递 + 配置） |
| **IntegrationTest** | 全流程自动化集成测试，共计 **385 项测试**，覆盖六大场景 |

#### 集成测试场景

| 场景文件 | 测试数 | 覆盖内容 |
|---------|:------:|---------|
| `full_scenario.in` | 82 | 快递完整生命周期：NORMAL/FRAGILE/BOOK 三种包裹类型的完整流程（发件→分配→揽收→签收），资金流动验证（寄件人扣款→公司池收款→快递员佣金结算），`GET_STATS` 全局统计校验，修改密码成功路径，删除已签收包裹，删除已完成用户 |
| `permission_test.in` | 48 | 11 种权限越界场景：客户越权分配/揽收/删除/查询越界，快递员越权签收/统计/发件，未登录操作拦截，客户 queryUsers 传错 type 等 |
| `edge_case_test.in` | 89 | 18 种异常/边界场景：重复注册、错误密码、余额不足、充值负数、包裹/用户不存在、脏数据删除拦截、跨用户越权操作、重复登录、分配已揽收包裹、重复签收、删除管理员、删除不存在包裹等 |
| `large_packet_test.in` | 43 | 大数据包传送：150 个预置包裹（响应 > 4096 字节），大数据量全生命周期业务操作，数据完整性验证 |
| `reconnect_test.in` | 68 | 断线重连：服务器运行中重启 + `RECONNECT` 恢复会话，重连后业务连续性，3 轮多重启/重连，切换用户重连 |
| `keepalive_test.in` | 55 | 连接保活/超时：`max_idle_time` 超时断开（3s），活动连接不被误断，超时断开后 RECONNECT 恢复，恢复默认 180s 超时 |

#### 集成测试架构

`IntegrationTest` 是一个脚本解释器模式的测试框架：

1. 在隔离目录 (`output/test_run/`) 生成预制测试数据文件
2. 以子进程启动 `server.exe`，自动探测并清理残留进程
3. 通过 `Communication` 类连接服务端，逐行解析 `.in` 脚本
4. 每行执行一个操作并断言 `ErrorCode`，支持 `PCL*` 通配符引用动态包裹 ID
5. 验证资金流向：`CHECK_BALANCE` 校验各角色余额，`GET_STATS` 校验全局统计
6. 测试完成后终止 server 进程，清理临时目录，**不污染生产数据**

#### 集成测试支持的命令

| 命令 | 格式 | 说明 |
|------|------|------|
| `REGISTER` | `REGISTER\|user\|pass\|name\|phone\|addr\|type\|expectedCode` | 注册用户 |
| `LOGIN` | `LOGIN\|user\|pass\|type\|expectedCode` | 登录 |
| `SEND` | `SEND\|receiver\|type\|weight\|desc\|expectedCode` | 发送快递，成功时自动记录 `PCL*` |
| `ASSIGN` | `ASSIGN\|parcelId\|courier\|expectedCode` | 分配快递员 |
| `COLLECT` | `COLLECT\|id1,id2,...\|expectedCode` | 批量为快递员揽收 |
| `SIGN` | `SIGN\|id1,id2,...\|expectedCode` | 批量签收快递 |
| `RECHARGE` | `RECHARGE\|amount\|expectedCode` | 充值 |
| `CHANGE_PWD` | `CHANGE_PWD\|oldPwd\|newPwd\|expectedCode` | 修改密码 |
| `QUERY_PARCELS` | `QUERY_PARCELS\|id\|sender\|receiver\|courier\|status\|start\|end\|expectedCode\|expectedCount` | 查询快递并断言结果数 |
| `QUERY_USERS` | `QUERY_USERS\|username\|type\|expectedCode\|expectedCount` | 查询用户并断言结果数 |
| `QUERY_BALANCE` | `QUERY_BALANCE\|expectedValue` | 查询余额并断言数值 |
| `CHECK_BALANCE` | `CHECK_BALANCE\|expectedValue` | 与 QUERY_BALANCE 同义 |
| `DELETE_USER` | `DELETE_USER\|username\|expectedCode` | 删除用户 |
| `DELETE_PARCEL` | `DELETE_PARCEL\|parcelId\|expectedCode` | 删除快递 |
| `GET_STATS` | `GET_STATS\|expectedCode\|users\|parcels\|pending\|collected\|signed\|balance` | 获取统计并断言全字段 |
| `LOGOUT` | `LOGOUT\|expectedCode` | 注销登录 |
| `WAIT` | `WAIT\|ms` | 等待指定毫秒数 |
| `RESTART_SERVER` | `RESTART_SERVER\|expectedCode` | 终止并重启服务端进程 |
| `RECONNECT` | `RECONNECT\|expectedCode` | 断开后重连并自动登录 |
| `GEN_PARCELS` | `GEN_PARCELS\|count\|type\|sender\|receiver\|weight\|desc` | 向 parcels.dat 追加预置包裹 |
| `CONFIG` | `CONFIG\|key\|value` | 修改 server_config.txt 配置项 |
| `PRINT` | `PRINT\|message` | 输出信息到测试日志 |

#### 资金流动验证

```
发件: 寄件人 -= price        公司池 += price
揽收: 快递员 += price * 0.5   公司池 -= price * 0.5
签收: 无资金变动
```

## 🔐 默认管理员

| 用户名 | 密码 | 角色 |
|-------|------|------|
| `admin` | `admin123` | Administrator |

系统首次启动时自动创建默认管理员账户。建议首次登录后立即修改密码。

---

## 🔍 代码质量说明

### 良好的设计实践

| 实践 | 说明 |
|------|------|
| **分层架构** | 表示层 / 控制层 / 业务&数据层 / 持久化层职责清晰，见架构说明 |
| **面向接口编程** | User / Parcel 基类 + 多态派生（NormalParcel / FragileParcel / BookParcel），`getPrice()` 虚函数实现差异化计费 |
| **协议编解码** | `Protocol` 类统一封装 `buildRequest()` / `parseRequest()`，通信双方复用同一套编解码逻辑 |
| **CMake 模块化** | Common 静态库被所有可执行文件共享，避免代码重复；每个子模块独立 CMakeLists.txt |
| **单线程无锁并发** | select + 非阻塞 socket 实现单线程多客户端，天然无竞态条件，无需锁/同步 |
| **内存工作单元** | `LogisticsSystem` 全量加载数据到内存运行，`FileManager` 仅作序列化工具，避免 Repository 透传冗余 |
| **安全数据解析** | `parseInt` / `parseDouble` / `parseLongLong` 带完整错误校验，拒绝非法格式 |
| **请求追踪日志** | 每个请求分配唯一递增 ID，日志中可追踪从收到到响应的完整链路 |
| **异常隔离** | 请求处理外层包裹 `try-catch`，防止单个请求异常导致整个服务崩溃 |
| **事务一致性** | 数据文件带版本头（V=N），每次写操作采用临时文件 + `rename()` 原子重命名，saveData 三文件同步递增版本号 |
| **自动重连** | 客户端 `Communication` 保存登录凭据，断线后通过 `reconnectAndRelogin()` 自动恢复会话 |
| **空闲超时检测** | select 超时（1s）周期扫描全部客户端活跃时间，超过 `max_idle_time` 的僵尸连接自动断开 |
| **连接保活** | 服务端 send 循环处理部分发送（非阻塞 EWOULDBLOCK 重试），单次 recv 后累积缓冲区处理半包/粘包 |
| **优雅关闭** | 支持 Ctrl+C / Ctrl+Break 信号处理，安全释放所有资源 |

### 待改进方向

| 方向 | 优先级 | 说明 |
|------|:------:|------|
| **密码安全** | 中 | 当前密码明文存储，建议引入 SHA-256 + Salt 哈希 |
| **handleClientRequest 公平性** | 中 | `while(true)` 循环连续处理同一连接的所有报文，不返回 select 主循环，可能导致其他连接被饿死。建议每次调用限制处理一个请求，或设置单次最大处理数量 |
| **裸指针管理** | 低 | `m_users` / `m_parcels` 存储裸指针，手动 `new`/`delete`。已确认析构时正确释放，异常路径下仍有泄漏风险。建议改用 `std::unique_ptr` 自动管理生命周期 |
| **查询快递可能重复** | 低 | `ClientHandler::handleQueryParcel` 中客户作为寄件人和收件人分别查询后合并结果未去重，同一用户同时为寄收件人时重复 |
| **非阻塞 socket 跨平台** | 低 | `server.cpp` 中 `FIONBIO` / `O_NONBLOCK` 仅覆盖 Windows 和 Linux，macOS/FreeBSD 等平台未处理 |
| **测试数据路径脆弱** | 低 | `dataset_tests.cpp` 的 `findDataDir()` 通过枚举多个候选路径定位数据目录，依赖当前工作目录 |
| **并发演进** | 低 | 架构已预留上下文设计（`ClientHandler` per-connection），未来可升级为多线程处理 |

> **已修复**: `deleteParcel` 未释放内存（P1）、`parseDouble` 缺 idx 校验（P2-3）、`Communication.cpp` 用 `stoi` 解析 time_t（P2-4）、根目录死代码清理、测试 CMake 引用缺失文件（P0）、客户端响应累积缓冲区 + `\n` 边界检测、服务端 send 循环处理部分发送、连接空闲超时断开（180s）、`handleRequst` 拼写修正、断线重连机制、saveData 临时文件 + `rename()` 原子替换 + 版本头事务保护、`loadParcels` 多余 `getline` 空字段问题。

---

## 📄 许可

本项目仅供学习参考。
