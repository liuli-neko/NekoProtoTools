# NekoProtoTools: 一个 C++ 协议辅助库

[![中文](https://img.shields.io/badge/语言-中文-blue.svg)](./README.md)
[![English](https://img.shields.io/badge/language-English-blue.svg)](./README_en.md)
![Version](https://img.shields.io/badge/version-0.3.0-green.svg)

### CI 状态

| 构建平台 | 状态| 代码覆盖率|
| :------- | :------------ | :------------ |
| Linux    | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) | [![codecov](https://codecov.io/gh/liuli-neko/NekoProtoTools/graph/badge.svg?token=F5OR647TV7)](https://codecov.io/gh/liuli-neko/NekoProtoTools) |
| Windows  | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml) | |
| Clang-cl | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml) | | 

---

## 1. 简介

NekoProtoTools 是一个纯 C++ 实现的协议辅助库，旨在**简化 C++ 中消息（协议）的定义、序列化/反序列化以及 RPC 通信**。

当前版本：**0.3.0**。这一版完成了序列化后端重构：RapidJSON、simdjson、Binary、XML 和 schema 生成都接入同一套通用 Parser 分发层，具体格式后端只负责 Reader/Writer 能力。

本库的核心特性：

*   **简化消息定义**：通过模板和宏，您只需定义消息字段，并声明需要处理的成员，即可将自定义 C++ 类型用作可序列化对象或协议消息。
*   **统一 Parser 序列化层**：JSON、Binary、XML 与 schema 生成共享同一套类型规则；RapidJSON、simdjson、pugixml 和二进制格式只是薄后端。
*   **字段级元数据 tags**：`make_tags<Tag>(field)` 用于描述字段或调用点，而不是给类型永久打标签，便于同一类型在不同上下文中使用不同布局。
*   **基础静态反射能力**：提供简单的静态反射机制，帮助在编译期进行类型检查和元数据提取。
*   **轻量级 JSON-RPC 2.0 实现**：基于模板和简洁接口，可以方便地定义和实现 RPC 服务。
*   **类型安全**：利用 C++ 模板元编程，在编译期进行类型检查。

**为什么需要这个库**：开发者应将精力聚焦于业务逻辑本身，而非繁琐、易错的协议构造和维护。本库致力于提供一个稳定、可测试的基础设施，降低协议处理的复杂度。

**使用这个库的示例:**
1. [IliasMySql](https://github.com/Btk-Project/IliasMySql): 
    * 一个基于 Ilias 协程的 MySQL 客户端，使用 NekoProtoTools 进行静态反射。

2. [coro-cpp-mcp](https://github.com/liuli-neko/coro-cpp-mcp): 
    * 一个基于 Ilias 协程的 mcp-sdk c++实现，使用 NekoProtoTools 进行jsonrpc处理。

---

## 2. 依赖项

*   **C++ 标准**：
    *   推荐 C++23；项目当前 xmake 默认 `stdcxx=23`，并保留 C++20/C++26 配置入口。
*   **序列化后端**：
    *   JSON：[RapidJSON](https://rapidjson.org/) 或 [simdjson](https://simdjson.org/) (可选，按需包含)。同时开启时，默认 `JsonSerializer` 会优先选择 RapidJSON；也可以直接使用具体后端类型。
    *   XML: [pugixml](https://pugixml.org/) (可选，按需包含)
    *   Binary：内置二进制 Reader/Writer，无需额外第三方库。
    *   *注意：为了保持库的轻量化，这些序列化库**并未直接捆绑**在本库中，您需要通过选项自行管理这些依赖。*
*   **通信与 RPC (可选)**：
    *   [Ilias](https://github.com/BusyStudent/ilias) (用于网络通信和异步任务)
*   **日志 (可选)**:
    *   [format](https://en.cppreference.com/w/cpp/utility/format/format) (如果编译环境中std::format可用，可以直接开启日志)
    *   [fmt](https://fmt.dev/) (使用fmt库作为日志后端)
    *   [spdlog](https://github.com/gabime/spdlog) (使用spdlog作为日志后端)


---

## 3. 安装与集成 (示例)

本库推荐使用 [xmake](https://xmake.io/) 进行构建和集成。

在您的 `xmake.lua` 中添加依赖：

```lua
-- xmake.lua
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")

add_requires("neko-proto-tools", {
    configs = {
         enable_simdjson = false, 
         enable_rapidjson = true,
         enable_pugixml = false,
         enable_fmt = true, 
         enable_spdlog = false,
         enable_communication = true,
         enable_jsonrpc = true,
         enable_protocol = true,
    }
})

target("your_project")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("neko-proto-tools")
```

---

## 4. 快速开始

### 4.1. 基本序列化/反序列化

只需要包含基础头文件，并使用 `NEKO_SERIALIZER` 宏标记需要序列化的成员。

```cpp
#include <nekoproto/serialization/serializer_base.hpp>
#include <nekoproto/serialization/json_serializer.hpp> // 使用 JSON 序列化器
#include <iostream>
#include <string>
#include <vector>

// 定义你的数据结构
struct MyData {
    int         id;
    std::string name;
    double      score;

    // 声明需要序列化的成员
    NEKO_SERIALIZER(id, name, score);
};

int main() {
    using namespace NekoProto; // 引入命名空间

    MyData data_out;
    data_out.id = 101;
    data_out.name = "Neko";
    data_out.score = 99.5;

    // --- 序列化 ---
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer serializer(buffer); // 创建 JSON 输出序列化器
    serializer(data_out); // 对 data_out 对象进行序列化
    serializer.end();     // 结束序列化，刷新缓冲区 (也可通过析构自动完成)

    std::string json_str(buffer.begin(), buffer.end());
    std::cout << "Serialized JSON: " << json_str << std::endl;
    // 输出: Serialized JSON: {"id":101,"name":"Neko","score":99.5}

    // --- 反序列化 ---
    MyData data_in;
    JsonSerializer::InputSerializer deserializer(buffer.data(), buffer.size()); // 创建 JSON 输入序列化器
    // 将 buffer 中的 JSON 数据反序列化到 data_in
    if (deserializer(data_in)) { // 检查反序列化是否成功
        std::cout << "Deserialized Data: id=" << data_in.id
                  << ", name=" << data_in.name
                  << ", score=" << data_in.score << std::endl;
        // 输出: Deserialized Data: id=101, name=Neko, score=99.5
    } else {
        std::cerr << "Deserialization failed!" << std::endl;
    }

    return 0;
}
```

### 4.2. 定义协议消息 (带反射和多态)

如果需要协议管理、反射和多态支持，需要包含 `proto_base.hpp` 并使用 `NEKO_DECLARE_PROTOCOL` 宏。

```cpp
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/serialization/serializer_base.hpp>
#include <nekoproto/serialization/json_serializer.hpp> // 指定默认序列化器
#include <iostream>
#include <string>
#include <vector>

// 定义协议消息结构体
struct UserProfile {
    int         userId;
    std::string username;

    // 1. 声明序列化成员
    NEKO_SERIALIZER(userId, username);
    // 2. 声明为协议，并指定默认序列化器
    NEKO_DECLARE_PROTOCOL(UserProfile, JsonSerializer);
};

struct LoginRequest {
    std::string username;
    std::string password_hash;

    NEKO_SERIALIZER(username, password_hash);
    NEKO_DECLARE_PROTOCOL(LoginRequest, JsonSerializer);
};


int main() {
    using namespace NekoProto;

    // --- 使用工厂创建和管理协议 ---
    // 创建工厂时可指定协议版本 (主版本, 次版本, 补丁版本)
    ProtoFactory factory(1, 0, 0);

    // 1. 创建 UserProfile 实例 (通过协议基类接口)
    // emplaceProto 返回一个包装了具体协议对象的 IProto 接口指针
    auto user_iproto = UserProfile::emplaceProto(123, "Alice");

    // 序列化为字节数据 (使用协议指定的默认序列化器 JsonSerializer)
    std::vector<char> user_data = user_iproto.toData();
    std::cout << "UserProfile Serialized Size: " << user_data.size() << " bytes" << std::endl;

    // 2. 通过工厂按名称创建协议实例
    IProto login_iproto = factory.create("LoginRequest"); // 返回 IProto 接口
    if (!login_iproto) {
         std::cerr << "Failed to create LoginRequest instance!" << std::endl;
         return 1;
    }

    // 使用 cast<T>() 安全地转换为具体类型指针 (如果类型不匹配会返回 nullptr)
    auto login_req = login_iproto.cast<LoginRequest>();
    if (login_req) {
        login_req->username = "Bob";
        login_req->password_hash = "some_hash_value";
    }

    std::vector<char> login_data = login_iproto.toData();
    std::cout << "LoginRequest Serialized Size: " << login_data.size() << " bytes" << std::endl;


    // 3. 从数据反序列化 (假设我们收到了 UserProfile 的数据)
    IProto received_proto = factory.create("UserProfile"); // 先创建对应类型的空实例
    if (received_proto.fromData(user_data)) { // 从数据填充
        std::cout << "Successfully deserialized data into " << received_proto.name() << std::endl;
        auto received_user = received_proto.cast<UserProfile>();
        if (received_user) {
            std::cout << "Received User ID: " << received_user->userId << std::endl;
            // 输出: Received User ID: 123
        }
    } else {
        std::cerr << "Failed to deserialize user data." << std::endl;
    }

    return 0;
}
```

---

## 5. 核心概念说明

### 5.1. 序列化器 (`Serializer`)

序列化器负责将 C++ 对象转换为字节流（序列化）以及将字节流转换回 C++ 对象（反序列化）。从 0.3.0 开始，类型分发规则集中在通用 `Parser<Reader, Writer, T>` 层；具体格式后端只提供 Reader/Writer 和少量可选能力。

*   **内置序列化器**：
    *   `JsonSerializer`: 默认 JSON 序列化器别名。开启 RapidJSON 时使用 `RapidJsonSerializer`；未开启 RapidJSON 但开启 simdjson 时使用 `SimdJsonSerializer`。
    *   `BinarySerializer`: 提供紧凑的二进制序列化格式，并支持 `fixedLength`、`unframed` 等二进制布局 tag。
    *   `XmlSerializer`: 使用 pugixml 提供 XML 序列化和反序列化支持。
*   **选择序列化器**：
    *   对于基本序列化，直接实例化所需的 `OutputSerializer` / `InputSerializer`。
    *   对于协议消息 (`NEKO_DECLARE_PROTOCOL`)，在声明时指定默认序列化器。
*   **schema 生成**：`include <nekoproto/serialization/json/schema.hpp>` 后可通过 `generate_schema<T>(schema)` 生成 Draft-07 风格 JSON Schema。
*   **扩展序列化**：通过 `detail::CustomParser<R, W, T>` 支持自定义类型；新格式后端实现 Reader/Writer 接口。详见 [7. 自定义序列化扩展](#7-自定义序列化扩展)。

### 5.2. 字段 tags 与调用点元数据

`make_tags<Tag>(value_or_accessor)` 用于描述“这个字段在当前绑定关系中如何被处理”。它不是类型自己的永久属性，因此同一个类型可以在不同结构、不同后端或不同顶层调用中使用不同 tags。

```cpp
#include <nekoproto/serialization/binary_serializer.hpp>
#include <cstdint>

struct Header {
    std::uint32_t length = 0;
    std::uint16_t type = 0;

    NEKO_SERIALIZER(make_tags<BinaryTags{.fixedLength = sizeof(std::uint32_t)}>(length),
                    make_tags<BinaryTags{.fixedLength = sizeof(std::uint16_t)}>(type))
};

std::vector<char> buffer;
BinarySerializer::OutputSerializer out(buffer);
out(make_tags<BinaryTags{.unframed = true}>(Header{12, 3}));
out.end();
```

常用内置 tag：

*   `JsonTags{.flat = true}`：反射对象字段展开到父对象。
*   `JsonTags{.skipable = true}`：读取缺失字段时允许跳过。
*   `JsonTags{.rawString = true}`：JSON 字符串按原始 JSON 片段处理。
*   `BinaryTags{.fixedLength = N}`：二进制基础类型按固定宽度读写。
*   `BinaryTags{.unframed = true}`：二进制反射对象按无字段边界布局读写；适合通信帧头这类调用点，不应作为类型级属性。
*   `rename_tag<"...">` / `comment_tag<"...">`：用于字段重命名和 XML 注释等反射元数据。

### 5.3. 协议管理 (`ProtoFactory`, `IProto`)

当需要管理不同类型的协议、进行多态处理或需要运行时类型信息时，使用协议管理机制。

*   **`NEKO_DECLARE_PROTOCOL(ClassName, DefaultSerializer)`**：
    *   将一个类注册为协议。
    *   使其可以使用 `IProto` 的接口。
    *   在 `ProtoFactory` 中自动注册，可通过名称或类型 ID 创建。
    *   指定该协议默认使用的序列化器。
*   **`ProtoFactory`**：
    *   协议工厂类，负责根据协议名称或类型 ID 创建协议实例 (`IProto`)。
    *   管理协议的版本信息。
    *   自动注册所有使用 `NEKO_DECLARE_PROTOCOL` 声明的协议（按字典序）。
*   **`IProto`**：
    *   协议的抽象基类接口。
    *   提供 `toData()` (序列化), `fromData()` (反序列化), `name()`, `type()` 等通用方法。
    *   使用 `emplaceProto()` 创建实例，返回 `IProto`。
    *   使用 `cast<T>()` 安全地将 `IProto` 转换回具体的协议类型指针。

### 5.4. 通信 (`Communication`)

本库提供了基于 [Ilias](https://github.com/BusyStudent/ilias) 的协程通信抽象层，用于方便地在网络连接上传输协议消息。

*   **需要头文件**: `#include <nekoproto/communication/communication_base.hpp>`
*   **核心类**: `ProtoStreamClient<UnderlyingStream>` (例如 `ProtoStreamClient<TcpClient>`)
*   **功能**: 封装了底层的网络流（如 TCP, UDP），自动处理协议消息的打包（添加类型信息和长度）、发送、接收和解包。

**示例 (TCP 通信)**:

> 完整示例代码: `tests/unit/communication/test_communication.cpp`

```cpp
#include <nekoproto/communication/communication_base.hpp>
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/serialization/json_serializer.hpp>

#include <ilias/net.hpp>         // Ilias 网络库
#include <ilias/platform.hpp>  // Ilias 平台上下文
#include <ilias/task.hpp>        // Ilias 协程/任务

#include <iostream>
#include <vector>
#include <string>
#include <chrono> // 用于时间戳

NEKO_USE_NAMESPACE // 使用 nekoproto 命名空间
using namespace ilias; // 使用 ilias 命名空间

// 定义要传输的消息协议
class ChatMessage {
public:
    uint64_t    timestamp;
    std::string sender;
    std::string content;

    NEKO_SERIALIZER(timestamp, sender, content);
    NEKO_DECLARE_PROTOCOL(ChatMessage, JsonSerializer); // 声明为协议
};

// 简单的服务器协程
ilias::Task<> server_task(PlatformContext& ioContext, ProtoFactory& protoFactory) {
    TcpListener listener(ioContext, AF_INET);
    listener.bind(IPEndpoint("127.0.0.1", 12345));
    std::cout << "Server listening on 127.0.0.1:12345" << std::endl;

    auto client_conn = co_await listener.accept(); // 等待连接
    std::cout << "Client connected." << std::endl;

    // 将 TCP 连接包装成协议流客户端 (服务器端视角)
    ProtoStreamClient<TcpClient> proto_stream(protoFactory, ioContext, std::move(client_conn.value().first));

    // 接收消息
    auto recv_result = co_await proto_stream.recv();
    if (recv_result) {
        IProto received_proto = std::move(recv_result.value());
        std::cout << "Server received protocol: " << received_proto.name() << std::endl;
        auto msg = received_proto.cast<ChatMessage>();
        if (msg) {
            std::cout << "  Timestamp: " << msg->timestamp << std::endl;
            std::cout << "  Sender: " << msg->sender << std::endl;
            std::cout << "  Content: " << msg->content << std::endl;

            // 回复一条消息
            ChatMessage reply_msg;
            reply_msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            reply_msg.sender = "Server";
            reply_msg.content = "Message received!";
            co_await proto_stream.send(reply_msg.makeProto()); // 发送协议
            std::cout << "Server sent reply." << std::endl;
        }
    } else {
        std::cerr << "Server failed to receive message: " << recv_result.error().message() << std::endl;
    }

    proto_stream.close(); // 关闭连接
    listener.close();
    std::cout << "Server task finished." << std::endl;
}

// 简单的客户端协程
ilias::Task<> client_task(PlatformContext& ioContext, ProtoFactory& protoFactory) {
    TcpClient tcpClient(ioContext, AF_INET);
    auto conn_result = co_await tcpClient.connect(IPEndpoint("127.0.0.1", 12345)); // 连接服务器
    if (!conn_result) {
        std::cerr << "Client failed to connect: " << conn_result.error().message() << std::endl;
        co_return;
    }
    std::cout << "Client connected to server." << std::endl;

    // 将 TCP 连接包装成协议流客户端
    ProtoStreamClient<TcpClient> proto_stream(protoFactory, ioContext, std::move(tcpClient));

    // 准备发送的消息
    ChatMessage msg_to_send;
    msg_to_send.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    msg_to_send.sender = "Client";
    msg_to_send.content = "Hello from client!";

    // 发送消息 (makeProto() 创建 IProto 实例)
    // SerializerInThread 选项表示序列化在其他线程进行
    auto send_result = co_await proto_stream.send(msg_to_send.makeProto(), ProtoStreamClient<TcpClient>::SerializerInThread);
    if (!send_result) {
         std::cerr << "Client failed to send message: " << send_result.error().message() << std::endl;
         co_await proto_stream.close();
         co_return;
    }
     std::cout << "Client sent message." << std::endl;

    // 接收回复
    auto recv_result = co_await proto_stream.recv();
    if (recv_result) {
        IProto received_proto = std::move(recv_result.value());
        std::cout << "Client received protocol: " << received_proto.name() << std::endl;
        auto reply_msg = received_proto.cast<ChatMessage>();
        if (reply_msg) {
             std::cout << "  Reply Content: " << reply_msg->content << std::endl;
        }
    } else {
         std::cerr << "Client failed to receive reply: " << recv_result.error().message() << std::endl;
    }

    co_await proto_stream.close(); // 关闭连接
    std::cout << "Client task finished." << std::endl;
}

int main() {
    PlatformContext ioContext;    // Ilias 协程上下文
    ProtoFactory protoFactory(1, 0, 0); // 协议工厂

    // 运行服务器和客户端任务
    ilias_go server_task(ioContext, protoFactory);
    ilias_go client_task(ioContext, protoFactory);

    ioContext.run(); // 运行 Ilias 事件循环直到所有任务完成

    return 0;
}
```

### 5.5. JSON-RPC 2.0

本库提供了一个基于 Ilias 的轻量级 JSON-RPC 2.0 实现，可以方便地定义和调用远程过程。

*   **需要头文件**: `#include <nekoproto/jsonrpc/jsonrpc.hpp>`
*   **核心概念**:
    *   **RPC 模块定义**: 使用 `struct` 定义一个包含所有 RPC 方法的模块。
    *   **`RpcMethod<Signature, "method_name">`**: 在模块中声明一个 RPC 方法，指定其函数签名 (`Signature`) 和在 JSON-RPC 协议中的方法名 (`method_name`)。支持所有可序列化的类型作为参数和返回值。
    *   **`JsonRpcServer<ModuleType>`**: RPC 服务器类，用于托管 RPC 模块并处理客户端请求。
    *   **`JsonRpcClient<ModuleType>`**: RPC 客户端类，用于连接服务器并调用远程方法。
    *   **异步支持**: 方法实现可以返回 `ilias::Task<ResultType>` 以支持异步操作。
*  **服务端内建方法**: 
    * rpc.get_method_list:      获取当前服务端所有方法列表
    * rpc.get_bind_method_list: 获取当前服务端所有绑定方法列表
    * rpc.get_method_info:      获取指定方法的描述
    * rpc.get_method_info_list: 获取所有方法的描述

**示例**:

```cpp
#include <nekoproto/jsonrpc/jsonrpc.hpp>
#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <numeric> // for std::accumulate
#include <iostream>
#include <vector>
#include <string>

NEKO_USE_NAMESPACE
using namespace ilias;

// 1. 定义 RPC 服务模块及方法
struct CalculatorModule {
    // 方法名 "add", 接受两个 int, 返回 int
    RpcMethod<int(int, int), "add"> add;
    // 方法名 "sum", 接受 vector<int>, 返回 int
    RpcMethod<int(std::vector<int>), "sum"> sum;
};

// 2. 实现服务器逻辑
ilias::Task<> run_server(PlatformContext& context) {
    JsonRpcServer<CalculatorModule> rpc_server; // 创建服务器实例

    // --- 绑定方法实现 ---
    // 可以使用 lambda, 函数指针, std::function 等
    rpc_server->add = [](int a, int b) -> int {
        std::cout << "Server: add(" << a << ", " << b << ") called." << std::endl;
        return a + b;
    };

    // 协程方法绑定，返回 ilias::IoTask<>
    rpc_server->sum = [](std::vector<int> vec) -> ilias::IoTask<int> {
        std::cout << "Server: sum(...) called asynchronously." << std::endl;
        int result = std::accumulate(vec.begin(), vec.end(), 0);
        // 可以通过co_await 来执行协程函数
        co_return result; // 使用 co_return 返回结果
    };

    // 启动服务器，监听指定地址和端口
    std::string listen_address = "tcp://127.0.0.1:12335";
    auto start_result = co_await rpc_server.start(listen_address);
    if (!start_result) {
        std::cerr << "Server failed to start: " << start_result.error().message() << std::endl;
        co_return;
    }
    std::cout << "RPC Server listening on " << listen_address << std::endl;

    // 等待服务器停止信号 (例如 Ctrl+C)
    co_await rpc_server.wait();
    std::cout << "RPC Server stopped." << std::endl;
}

// 3. 实现客户端逻辑
ilias::Task<> run_client(PlatformContext& context) {
    JsonRpcClient<CalculatorModule> rpc_client; // 创建客户端实例

    // 连接到服务器
    std::string server_address = "tcp://127.0.0.1:12335";
    auto connect_result = co_await rpc_client.connect(server_address);
    if (!connect_result) {
        std::cerr << "Client failed to connect: " << connect_result.error().message() << std::endl;
        co_return;
    }
    std::cout << "Client connected to " << server_address << std::endl;

    // --- 调用远程方法 ---
    // 调用 add 方法
    auto add_result = co_await rpc_client->add(10, 5);
    if (add_result) {
        std::cout << "Client: add(10, 5) = " << add_result.value() << std::endl; // 输出: 15
    } else {
        std::cerr << "Client: add call failed: " << add_result.error().message() << std::endl;
    }

    // 调用 sum 方法 (异步)
    std::vector<int> nums = {1, 2, 3, 4, 5};
    auto sum_result = co_await rpc_client->sum(nums);
    if (sum_result) {
        std::cout << "Client: sum({1,2,3,4,5}) = " << sum_result.value() << std::endl; // 输出: 15
    } else {
         std::cerr << "Client: sum call failed: " << sum_result.error().message() << std::endl;
    }

    // 调用一个不存在的方法 (预期失败)
    // 注意：直接调用未在 CalculatorModule 中定义的 rpc_client->multiply(2, 3) 会导致编译错误。
    // 若要模拟调用不存在的方法，需要手动构造 JSON-RPC 请求，这里不演示。

    rpc_client.close(); // 断开连接
    std::cout << "Client disconnected." << std::endl;
}

int main() {
    PlatformContext context;

    // 启动服务器和客户端任务
    ilias_go run_server(context);
    // 启动客户端
    ilias_go run_client(context);

    context.run(); // 运行事件循环

    // 在客户端任务完成后，可以停止服务器 (例如通过发送信号或设置标志)
    return 0;
}

```

---

## 6. 支持的类型

序列化后端会引入统一的 Parser 集合，常用 STL 容器和基础类型可直接使用，无需再包含单独的 `serialization/types/*.hpp`。0.3.0 后，JSON、Binary、XML 和 schema 生成会尽量复用同一套类型规则，只有格式确实无法表达的语义才由对应后端忽略或特化处理。

当前通用 Parser 覆盖的主要类型包括：算术类型、字符串、枚举、`std::optional`、指针、`std::variant`、`std::tuple`/`std::pair`、顺序容器、集合、映射、`std::array`、`std::bitset`、`std::atomic`、`std::byte`、`BinaryData<T>` 以及通过 `NEKO_SERIALIZER` 或 `Meta<T>` 暴露反射信息的结构体。

详细支持请参照 [Supported Types Overview](https://github.com/liuli-neko/NekoProtoTools/wiki/Supported-Types-Overview)。

## 7. 自定义序列化扩展

自定义 C++ 类型应特化 `detail::CustomParser<R, W, T>`。读写逻辑可复用已有 Parser，因此同一扩展可以用于 JSON、Binary 和 XML 后端。

```cpp
#include <nekoproto/serialization/parsing/parsers.hpp>

struct StrongId {
    std::uint64_t value = 0;
};

namespace NekoProto::detail {
template <typename R, typename W>
struct CustomParser<R, W, StrongId> {
    template <typename Parent, typename Tags>
    static ParserResult write(W& writer, const StrongId& id, const Parent& parent, const Tags& tags) {
        return parser_write<R, W>(writer, id.value, parent, tags);
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, StrongId& id, const Tags& tags) {
        return parser_read<R, W>(in, id.value, tags);
    }

    static parsing::schema::Type toSchema() {
        return parser_schema<R, W, std::uint64_t>();
    }
};
} // namespace NekoProto::detail
```

如果要增加一种新的数据格式，需实现与现有 `rapid::Reader/Writer`、`binary::Reader/Writer` 相同职责的后端，并由序列化器入口调用 `parser_write` / `parser_read`。

---

## 8. TODO

**序列化器 (Serializer)**

*   [x] 支持通过字符串名称访问协议字段 (基础反射)
*   [x] 使用 simdjson 作为 JSON 输入序列化器后端 (`simdjson::dom`)
*   [x] RapidJSON、simdjson、Binary、XML、schema 统一到通用 Parser 层
*   [x] 使用 pugixml 实现 XML Reader/Writer 后端
*   [x] 支持 Draft-07 风格 JSON Schema 生成
*   [ ] 支持 `simdjson::ondemand` 接口 (探索其与 `dom` 接口的性能和使用场景差异)
*   [x] 实现基于共享 JSON 文本 Writer 的 simdjson 输出路径
*   [x] 支持更多 C++ STL 容器


**通信 (Communication)**

*   [x] 支持 UDP 通信通道 (`ProtoDatagramClient`)
*   [ ] 支持更多底层传输协议 (如 WebSocket, QUIC - 可能通过 Ilias 或其他库集成)
*   [ ] 优化通信层原子性：确保数据帧的完整处理，即使在取消操作时也能保证数据流状态一致。可能需要调整为小帧发送机制。

**JSON-RPC**

*   [x] 支持 JSON-RPC 2.0 协议规范。
*   [x] 兼容 JSON-RPC 1.0 协议。
*   [ ] JSON-RPC 扩展。
*   [x] 新增服务器内置方法：
    - `rpc.get_method_list`: 获取当前服务端所有方法列表
    - `rpc.get_bind_method_list`: 获取当前服务端所有绑定方法列表
    - `rpc.get_method_info`: 获取指定方法信息
    - `rpc.get_method_info_list`: 获取所有方法信息列表
*   [x] 新增命名参数支持, 允许在声明或绑定方法时显示指定参数名称, 指定了名称的方法在调用时会以JsonObject的方式传递参数, 否则使用JsonArray的方式按位置传递。

---

## 9. 开发历史 (部分里程碑)
*   **v0.3.0**
    *   序列化后端重构：RapidJSON、simdjson、Binary、XML 和 schema 生成统一接入 `Parser<Reader, Writer, T>`。
    *   后端收敛为薄 Reader/Writer；旧 public 状态机 API 不再作为新后端扩展入口。
    *   反射 tags 从 value wrapper 迁移为独立字段/调用点元数据，删除旧 `TaggedValue` 用法。
    *   `BinaryTags::fixedLength`、`BinaryTags::unframed` 由实际消费层解释；`unframed` 不再是类型级 schema metadata。
    *   XML 切换为 pugixml 后端，支持对象字段、数组同名兄弟元素、节点文本和注释。
    *   通信层帧头 `MessageHeader` 不再注册进 `ProtoFactory`，由通信层显式以调用点 tag 读写。

*   **v0.2.6**
    *   完善tags支持，支持自定义tag。
    *   反射类新增元数据支持
        *   新增反射的成员类型元数据
        *   新增反射的成员tags元数据
    *   修复jsonrpc服务端发送空数据
    *   修复base64编码空数据崩溃

*   **v0.2.5**
    *   提升最低c++支持到c++20, 新增简单静态反射支持。
    *   jsonrpc模块重构。
        *   优化符号膨胀问题。所有接口声明为无异常抛出。
        *   重命名通信特化类名称。

*   **v0.2.4**
    *   分离序列化和协议管理，序列化作为核心必要模块，协议管理作为可选模块。
    *   完善 JSON-RPC 2.0 协议支持。
        *   支持命名参数传递。
        *   支持内置方法：`rpc.get_method_list`, `rpc.get_bind_method_list`, `rpc.get_method_info`, `rpc.get_method_info_list`。
    *   给类型增加跳过标记is_skipable。标识一个非optional的可跳过类型。
    *   优化jsonrpc模板膨胀，开启/bigobj选项编译

*   **v0.2.3**
    *   统一大部分序列化调用为括号表达式 `serializer(variable)`。
    *   调整 `NameValuePair` 与嵌套对象的节点展开规则。
    *   支持 simdjson 作为 JSON 输入序列化后端 (`simdjson::dom`)。
    *   几乎支持所有常用 STL 容器。
    *   增加 `std::optional` 的通用支持。
    *   修改通信接口，使其成为传输协议的包装层，底层连接管理分离。
    *   新增将通信过程中的序列化/反序列化推到其他线程执行的选项。
    *   新增编译选项以控制功能启用和日志输出。
    *   新增 UDP 通信支持。
    *   新增协议表同步支持 (发送端可分享协议表，接收端据此反序列化)。
    *   预留内部协议类型 ID (1-64) 用于控制信息。
    *   完善二进制协议，修复部分类型的序列化问题。
    *   协议传输中增加更多标志位 (Flag)，支持协议表交换、未知协议作为二进制包处理、流式切片传输与中断。
    *   支持基于 pugixml 的 XML 序列化和反序列化。
    *   新增 JSON-RPC 2.0 支持。

*   **v0.2.0 - alpha**
    *   修改序列化器接口，统一为重载的括号表达式。
    *   初步优化反序列化性能。
    *   减少协议基类空间开销。
    *   支持 TCP 通信通道。

*   **v0.1.0**
    *   协议基类改为组合方式。
    *   增加字段反射支持。
    *   增加 `std::optional`, `std::variant` 支持。

---

## 10. 贡献

欢迎提交 Pull Requests 或 Issues 来改进本库！

---

## 11. 许可证

![GitHub License](https://img.shields.io/github/license/liuli-neko/NekoProtoTools)
