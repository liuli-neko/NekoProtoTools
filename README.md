# NekoProtoTools: 一个 C++ 协议辅助库

[![中文](https://img.shields.io/badge/语言-中文-blue.svg)](./README.md)
[![English](https://img.shields.io/badge/language-English-blue.svg)](./README_en.md)

### CI 状态

| 构建平台 | 状态| 代码覆盖率|
| :------- | :------------ | :------------ |
| Linux    | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) | [![codecov](https://codecov.io/gh/liuli-neko/NekoProtoTools/graph/badge.svg?token=F5OR647TV7)](https://codecov.io/gh/liuli-neko/NekoProtoTools) |
| Windows  | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml) | |
| Clang-cl | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml) | | 

---

## 1. 简介

NekoProtoTools 是一个纯 C++ 实现的协议辅助库，旨在**简化 C++ 中消息（协议）的定义、序列化/反序列化以及 RPC 通信**。

本库的核心特性：

*   **简化消息定义**：通过模板和宏，您只需定义消息的字段，并声明需要处理的成员，即可将任意自定义 C++ 类型用作协议消息。
*   **开箱即用的序列化/反序列化**：内置支持 JSON (RapidJSON/SIMDJson)、二进制等多种常用序列化格式，并允许轻松扩展自定义序列化器。
*   **基础反射能力**：提供简单的反射机制，允许通过名称信息在运行时处理协议。
*   **轻量级 JSON-RPC 2.0 实现**：基于模板和简洁接口，可以方便地定义和实现 RPC 服务。
*   **类型安全**：利用 C++ 模板元编程，在编译期进行类型检查。

**为什么需要这个库**：开发者应将精力聚焦于业务逻辑本身，而非繁琐、易错的协议构造和维护。本库致力于提供一个稳定、可测试的基础设施，降低协议处理的复杂度。

---

## 2. 依赖项

*   **序列化后端**：
    *   JSON：[RapidJSON](https://rapidjson.org/) 或 [SIMDJson](https://simdjson.org/) (可选，按需包含)
    *   XML: [RapidXML](https://github.com/dwd/rapidxml) (可选，按需包含)
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
add_requires("neko-proto-tools", {
    configs = {
         enable_simdjson = false, 
         enable_rapidjson = true,
         enable_rapidxml = false, 
         enable_fmt = true, 
         enable_spdlog = false,
         enable_communication = true,
         enable_jsonrpc = true,
         enable_protocol = true,
    }
}) -- 可能需要指定 git repo 或其他来源(https://github.com/Btk-Project/xmake-repo.git)

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
#include <nekoproto/serialization/types/string.hpp>    // 支持 std::string
#include <nekoproto/serialization/types/vector.hpp>    // 支持 std::vector
// #include <nekoproto/serialization/types/types.hpp>  // 所有支持的类型
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
#include <nekoproto/serialization/types/string.hpp>
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

    // 序列化为二进制数据 (使用协议指定的默认序列化器 JsonSerializer)
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

序列化器负责将 C++ 对象转换为字节流（序列化）以及将字节流转换回 C++ 对象（反序列化）。

*   **内置序列化器**：
    *   `JsonSerializer`: 使用 RapidJSON 或 SIMDJson 进行 JSON 序列化/反序列化。
    *   `BinarySerializer`: 提供一种紧凑的二进制序列化格式。
    *   `XmlSerializer`: 提供 XML 反序列化支持 (目前仅反序列化)。
*   **选择序列化器**：
    *   对于基本序列化，直接实例化所需的 `OutputSerializer` / `InputSerializer`。
    *   对于协议消息 (`NEKO_DECLARE_PROTOCOL`)，在声明时指定默认序列化器。
*   **自定义序列化器**：可以实现 `detail::OutputSerializer<YourSerializer>` 和 `detail::InputSerializer<YourSerializer>` 接口来支持自定义格式。详见 [7. 自定义序列化器](#7-自定义序列化器)。

### 5.2. 协议管理 (`ProtoFactory`, `IProto`)

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

### 5.3. 通信 (`Communication`)

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
#include <nekoproto/serialization/types/string.hpp>
#include <nekoproto/serialization/types/vector.hpp> // 需要包含所有用到的类型支持头文件

#include <ilias/net.hpp>         // Ilias 网络库
#include <ilias/platform.hpp>  // Ilias 平台上下文
#include <ilias/task.hpp>        // Ilias 协程/任务

#include <iostream>
#include <vector>
#include <string>
#include <chrono> // 用于时间戳

NEKO_USE_NAMESPACE // 使用 nekoproto 命名空间
using namespace ILIAS_NAMESPACE; // 使用 ilias 命名空间

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

### 5.4. JSON-RPC 2.0

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
using namespace ILIAS_NAMESPACE;

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

本库通过专门的头文件为众多 C++ 标准库类型提供了序列化支持。
几乎所有的 STL 容器和常用类型都可以直接使用。详细支持请参照[Supported Types Overview](https://github.com/liuli-neko/NekoProtoTools/wiki/Supported-Types-Overview)。

## 7. 自定义序列化器

如果需要支持本库未内置的序列化格式，可以实现自定义序列化器。你需要继承 `detail::OutputSerializer<CustomOutputSerializer>` 和 `detail::InputSerializer<CustomInputSerializer>` 并实现基础类型的保存和加载函数。

实现细节请参考 [Implementing Custom Serializer](https://github.com/liuli-neko/NekoProtoTools/wiki/Implementing-Custom-Serializer)。

**输出序列化器接口**:

```cpp
#include <nekoproto/serialization/serializer_base.hpp>

using namespace nekoproto; // or nekoproto::detail

class CustomOutputSerializer : public detail::OutputSerializer<CustomOutputSerializer> {
public:
    // 构造函数，通常接收输出缓冲区
    CustomOutputSerializer(std::vector<char>& out_buffer);

    // --- 核心保存函数 ---
    // 保存各种基础类型
    bool saveValue(const int8_t value);
    bool saveValue(const uint8_t value);
    // ... 其他整数类型 (int16, uint16, int32, uint32, int64, uint64)
    bool saveValue(const float value);
    bool saveValue(const double value);
    bool saveValue(const bool value);
    bool saveValue(const std::string& value);
    // bool saveValue(const char* value); // 可选
    // bool saveValue(const std::string_view value); // 可选 (C++17+)

    // 保存带名称的值 (用于对象/struct成员)
    // 需要处理 T 为 std::optional 且无值的情况 (通常不输出该成员)
    template <typename T>
    bool saveValue(const NameValuePair<T>& value);

    // --- 容器/结构控制 ---
    // 开始数组/列表，size 为元素数量
    bool startArray(std::size_t size);
    // 结束数组/列表
    bool endArray();
    // 开始对象/结构体，size 为成员数量 (可能为 -1 如果无法预知)
    bool startObject(std::size_t size);
    // 结束对象/结构体
    bool endObject();

    // --- 特殊标记处理 ---
    // 保存容器大小标记 (通常在数组序列化时使用，和startArray时输入的数组长度一致)
    template <typename T>
    bool saveValue(SizeTag<T> const& size);

    // 结束整个序列化过程，确保所有缓冲都已写入
    // 析构函数也应确保调用 end() 或完成写入
    bool end();
};
```

**输入序列化器接口**:

```cpp
#include <nekoproto/serialization/serializer_base.hpp>

using namespace nekoproto; // or nekoproto::detail

class CustomInputSerializer : public detail::InputSerializer<CustomInputSerializer> {
public:
    // 构造函数，通常接收输入缓冲区
    CustomInputSerializer(const char* input_buffer, std::size_t size);
    // 或使用 string_view 等其他输入源
    // CustomInputSerializer(std::string_view input_data) : m_data(input_data) {}

    // --- 状态检查 ---
    // 返回序列化器是否处于有效状态 (例如，json解析失败了)
    explicit operator bool() const;

    // --- 核心加载函数 ---
    // 加载各种基础类型
    bool loadValue(int8_t& value);
    bool loadValue(uint8_t& value);
    // ... 其他整数类型 (int16, uint16, int32, uint32, int64, uint64)
    bool loadValue(float& value);
    bool loadValue(double& value);
    bool loadValue(bool& value);
    bool loadValue(std::string& value);

    // 加载带名称的值 (用于对象/struct成员)
    // 需要能根据名称查找并加载对应的值到 value.value 中
    template <typename T>
    bool loadValue(const NameValuePair<T>& value);

    // --- 容器/结构控制 ---
    // 进入一个节点/元素 (例如，进入数组)
    bool startNode();
    // 完成当前节点/元素的处理 (例如，数组已完整读取，退出当前数据并返回上一级)
    bool finishNode();

    // --- 特殊标记处理 (可选) ---
    // 加载容器大小标记 (通常在开始处理数组前调用)
    template <typename T>
    bool loadValue(const SizeTag<T>& value); // value.size 将被填充
};
```

---

## 8. TODO

**序列化器 (Serializer)**

*   [x] 支持通过字符串名称访问协议字段 (基础反射)
*   [x] 使用 SIMDJson 作为 JSON 输入序列化器后端 (`simdjson::dom`)
*   [ ] 支持 `simdjson::ondemand` 接口 (探索其与 `dom` 接口的性能和使用场景差异)
*   [x] 实现基于 SIMDJson 的 JSON 输出序列化器 (目前为手动实现，性能待优化)
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
*   **当前**
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
    *   `NameValuePair`, `SizeTag` 等特殊结构不再触发节点展开，其他无 `minimal_serializable` 属性的对象触发节点展开（类似 JSON 嵌套）。
    *   支持 SIMDJson 作为 JSON 输入序列化后端 (`simdjson::dom`)。
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
    *   支持 XML 反序列化。
    *   新增 JSON-RPC 2.0 支持。

*   **v0.2.0 - alpha**
    *   修改序列化器接口，统一为重载的括号表达式，替换 `Convert` 类为 `save`/`load` 函数。
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

