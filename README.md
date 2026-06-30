# NekoProtoTools: 一个 C++ 协议辅助库

[![中文](https://img.shields.io/badge/语言-中文-blue.svg)](./README.md)
[![English](https://img.shields.io/badge/language-English-blue.svg)](./README_en.md)
![Version](https://img.shields.io/badge/version-0.3.1-green.svg)

### CI 状态

| 构建平台 | 状态| 代码覆盖率|
| :------- | :------------ | :------------ |
| Linux    | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) | [![codecov](https://codecov.io/gh/liuli-neko/NekoProtoTools/graph/badge.svg?token=F5OR647TV7)](https://codecov.io/gh/liuli-neko/NekoProtoTools) |
| Windows  | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml) | |
| Clang-cl | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml) | | 

---

## 1. 简介

NekoProtoTools 是一个纯 C++ 实现的协议辅助库，旨在**简化 C++ 中消息（协议）的定义、序列化/反序列化以及 RPC 通信**。

当前版本：**0.3.1**。这一版新增反射驱动的命令行参数解析模块，并把 tags 基础设施进一步统一到全局反射元数据层；序列化侧的 `sa::Result<T>` 在支持 `<expected>` 的环境会直接使用 `std::expected<T, sa::Error>`，否则使用内置兼容实现。

本库的核心特性：

*   **简化消息定义**：业务类型只需定义字段；推荐通过非侵入 `template<> struct Meta<T>` 声明反射元数据，即可将自定义 C++ 类型用作可序列化对象或协议消息。
*   **统一 Parser 序列化层**：JSON、Binary、XML 与 schema 生成共享同一套类型规则；RapidJSON、simdjson、pugixml 和二进制格式只是薄后端。
*   **字段级元数据 tags**：`make_tags<Tag>(field)` 用于描述字段或调用点，而不是给类型永久打标签，便于同一类型在不同上下文中使用不同布局。
*   **反射驱动 ArgParser**：复用对象反射和 tags 定义命令行选项，支持嵌套选项、子命令、默认值、环境变量、别名、互斥/依赖关系和 help/version 输出。
*   **基础静态反射能力**：提供简单的静态反射机制，帮助在编译期进行类型检查和元数据提取。
*   **通用 RPC 前端与可替换后端**：RPC 方法声明、注册和调用不绑定具体 wire protocol；当前内置 JSON-RPC 后端和精简二进制 `BinaryRpcBackend`。
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
*   **命令行参数解析**：
    *   ArgParser 为头文件模块，无额外第三方依赖；包含 `<nekoproto/argparser/argparser.hpp>` 即可使用。


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

只需要包含基础头文件，并为类型提供非侵入的 `Meta<T>` 元数据即可。`NEKO_SERIALIZER` 宏仍可用于简单场景，但 README 主推 `Meta<T>`，便于把业务类型和库元数据分开。

```cpp
#include <nekoproto/serialization/reflection.hpp>
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
};

namespace NekoProto {
template <>
struct Meta<::MyData> {
    constexpr static auto value =
        Object("id", &::MyData::id, "name", &::MyData::name, "score", &::MyData::score);
};
} // namespace NekoProto

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

如果需要协议管理、反射和多态支持，需要包含 `proto_base.hpp` 并使用 `NEKO_DECLARE_PROTOCOL` 声明协议包装入口；字段元数据仍推荐通过外部 `Meta<T>` 定义。

```cpp
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/serializer_base.hpp>
#include <nekoproto/serialization/json_serializer.hpp> // 指定默认序列化器
#include <iostream>
#include <string>
#include <vector>

// 定义协议消息结构体
struct UserProfile {
    int         userId;
    std::string username;

    NEKO_DECLARE_PROTOCOL(UserProfile, JsonSerializer);
};

struct LoginRequest {
    std::string username;
    std::string password_hash;

    NEKO_DECLARE_PROTOCOL(LoginRequest, JsonSerializer);
};

namespace NekoProto {
template <>
struct Meta<::UserProfile> {
    constexpr static auto value =
        Object("userId", &::UserProfile::userId, "username", &::UserProfile::username);
};

template <>
struct Meta<::LoginRequest> {
    constexpr static auto value =
        Object("username", &::LoginRequest::username, "password_hash", &::LoginRequest::password_hash);
};
} // namespace NekoProto


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

序列化器负责将 C++ 对象转换为字节流（序列化）以及将字节流转换回 C++ 对象（反序列化）。当前类型分发规则集中在 `WriteParser<Writer, T>`、`ReadParser<Reader, T>` 和 `SchemaParser<T>` 三条路径；具体格式后端只提供 Reader/Writer 和少量可选能力。

*   **内置序列化器**：
    *   `JsonSerializer`: 默认 JSON 序列化器别名。开启 RapidJSON 时使用 `RapidJsonSerializer`；未开启 RapidJSON 但开启 simdjson 时使用 `SimdJsonSerializer`。
    *   `BinarySerializer`: 提供紧凑的二进制序列化格式，并支持 `fixed_length`、`unframed` 等二进制布局 tag。
    *   `XmlSerializer`: 使用 pugixml 提供 XML 序列化和反序列化支持。
*   **选择序列化器**：
    *   对于基本序列化，直接实例化所需的 `OutputSerializer` / `InputSerializer`。
    *   对于协议消息 (`NEKO_DECLARE_PROTOCOL`)，在声明时指定默认序列化器。
*   **schema 生成**：`include <nekoproto/serialization/json/schema.hpp>` 后可通过 `generate_schema<T>(schema)` 生成 Draft-07 风格 JSON Schema。
*   **扩展序列化**：通过 `CustomParser<T>` 支持自定义类型；新格式后端实现 Reader/Writer 接口。详见 [7. 自定义序列化扩展](#7-自定义序列化扩展)。

### 5.2. 字段 tags 与调用点元数据

`make_tags<Tag>(value_or_accessor)` 用于描述“这个字段在当前绑定关系中如何被处理”。它不是类型自己的永久属性，因此同一个类型可以在不同结构、不同后端或不同顶层调用中使用不同 tags。

```cpp
#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/binary_serializer.hpp>
#include <cstdint>

struct Header {
    std::uint32_t length = 0;
    std::uint16_t type = 0;
};

namespace NekoProto {
template <>
struct Meta<::Header> {
    constexpr static auto value =
        Object("length",
               make_tags<BinaryTag{.fixed_length = sizeof(std::uint32_t)}>(&::Header::length),
               "type",
               make_tags<BinaryTag{.fixed_length = sizeof(std::uint16_t)}>(&::Header::type));
};
} // namespace NekoProto

std::vector<char> buffer;
BinarySerializer::OutputSerializer out(buffer);
out(make_tags<BinaryTag{.unframed = true}>(Header{12, 3}));
out.end();
```

常用内置 tag：

*   `JsonTag{.flat = true}`：反射对象字段展开到父对象。
*   `JsonTag{.skippable = true}`：读取缺失字段时允许跳过。
*   `JsonTag{.raw_string = true}`：JSON 字符串按原始 JSON 片段处理。
*   `BinaryTag{.fixed_length = N}`：二进制基础类型按固定宽度读写。
*   `BinaryTag{.unframed = true}`：二进制反射对象按无字段边界布局读写；适合通信帧头这类调用点，不应作为类型级属性。
*   `rename_tag<"...">` / `comment_tag<"...">`：用于字段重命名和 XML 注释等反射元数据。

### 5.3. 命令行参数解析 (`ArgParser`)

ArgParser 复用本库的静态反射和 tags 体系，把一个普通配置结构体映射为命令行 schema。解析结果返回 `std::expected<T, std::error_code>`；`--help`、`--version`、缺失必填项和非法值都会通过 `ArgParserError` 表达。

```cpp
#include <nekoproto/argparser/argparser.hpp>

#include <string>
#include <vector>

using namespace NekoProto;
using namespace NekoProto::argparser;

struct BuildOptions {
    bool verbose = false;
    int jobs = 1;
    std::string mode = "debug";
    std::vector<std::string> include;
};

namespace NekoProto {
template <>
struct Meta<::BuildOptions> {
    constexpr static auto value =
        Object("verbose",
               make_tags<argparser::arg_name<"verbose", "v">,
                         argparser::arg_help<"enable verbose logs">,
                         argparser::ArgTags{.flag = true}>(&::BuildOptions::verbose),
               "jobs",
               make_tags<argparser::arg_name<"jobs", "j">,
                         argparser::arg_default<4>,
                         argparser::arg_help<"parallel jobs">,
                         argparser::ArgTags{.range_min = 1, .range_max = 65}>(&::BuildOptions::jobs),
               "mode",
               make_tags<argparser::arg_name<"mode", "m">,
                         argparser::arg_choices<"debug", "release">>(&::BuildOptions::mode),
               "include",
               make_tags<argparser::arg_name<"include", "I">,
                         argparser::arg_help<"include path">,
                         argparser::ArgTags{.repeatable = true}>(&::BuildOptions::include));
};
} // namespace NekoProto

int main(int argc, char** argv) {
    auto result = parser<BuildOptions>(argc, argv);
    if (!result) {
        return result.error() == make_error_code(ArgParserError::HelpRequested) ? 0 : 1;
    }

    const BuildOptions& options = *result;
    (void)options;
    return 0;
}
```

常用 arg tags 包括 `arg_name`、`arg_help`、`arg_default`、`arg_choices`、`arg_env`、`arg_separator`、`arg_aliases`、`arg_implicit`、`arg_group`、`arg_conflicts`、`arg_requires`、`arg_deprecated` 和 `arg_case_insensitive_choices`。字段可通过 `ArgTags{.positional = true}`、`.flag = true`、`.repeatable = true`、`.required = true` 等基础标记描述行为；子命令可通过 `ArgTags{.command = true}` 建模。

### 5.4. 协议管理 (`ProtoFactory`, `IProto`)

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

### 5.5. 通信 (`Communication`)

本库提供了基于 [Ilias](https://github.com/BusyStudent/ilias) 的协程通信抽象层，用于方便地在网络连接上传输协议消息。

*   **需要头文件**: `#include <nekoproto/communication/communication_base.hpp>`
*   **核心类**: `ProtoStreamClient<UnderlyingStream>` (例如 `ProtoStreamClient<TcpClient>`)
*   **功能**: 封装了底层的网络流（如 TCP, UDP），自动处理协议消息的打包（添加类型信息和长度）、发送、接收和解包。

**示例 (TCP 通信)**:

> 完整示例代码: `tests/unit/communication/test_communication.cpp`

```cpp
#include <nekoproto/communication/communication_base.hpp>
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/serialization/reflection.hpp>
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

    NEKO_DECLARE_PROTOCOL(ChatMessage, JsonSerializer); // 声明为协议
};

namespace NekoProto {
template <>
struct Meta<::ChatMessage> {
    constexpr static auto value =
        Object("timestamp", &::ChatMessage::timestamp,
               "sender", &::ChatMessage::sender,
               "content", &::ChatMessage::content);
};
} // namespace NekoProto

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

### 5.6. 通用 RPC 前端

RPC 模块已经拆成“通用前端 + 可替换后端”的结构。`RpcMethod`、协议集合、server/client 调用 API 不绑定 JSON-RPC；JSON-RPC 2.0 只是当前内置后端之一，负责 request/response 壳、id、batch、notification、参数编解码和错误映射。

*   **通用头文件**: `#include <nekoproto/rpc/rpc.hpp>`
*   **JSON-RPC 后端**: `#include <nekoproto/jsonrpc/backend.hpp>`，或直接包含兼容聚合头 `#include <nekoproto/jsonrpc/jsonrpc.hpp>`
*   **通用 API**: `RpcServer<Backend, ProtocolSets...>` / `RpcClient<Backend, ProtocolSets...>`
*   **快捷别名**: `JsonRpcServer<Api>` 等价于 `RpcServer<JsonRpcBackend, Api>`，`JsonRpcClient<Api>` 等价于 `RpcClient<JsonRpcBackend, Api>`
*   **异步支持**: 方法实现可以返回普通值、`void` 或 `ilias::IoTask<T>`；客户端调用统一返回 `ilias::IoTask<T>`

```cpp
#include <nekoproto/rpc/rpc.hpp>
#include <nekoproto/jsonrpc/backend.hpp>
#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/serializer_base.hpp>

#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <numeric>
#include <string>
#include <vector>

NEKO_USE_NAMESPACE

struct CalculatorApi {
    RpcMethod<int(int, int), "add", "lhs", "rhs"> add;
    RpcMethod<int(std::vector<int>), "sum", "items"> sum;
};

struct CommonApi {
    RpcMethod<std::string(), "version"> version;
};

struct AppApi {
    CalculatorApi calc;
    CommonApi common;
};

namespace NekoProto {
template <>
struct Meta<::CalculatorApi> {
    constexpr static auto value = Object("add", &::CalculatorApi::add, "sum", &::CalculatorApi::sum);
};

template <>
struct Meta<::CommonApi> {
    constexpr static auto value = Object("version", &::CommonApi::version);
};

template <>
struct Meta<::AppApi> {
    // calc 默认注册为 "calc.add" / "calc.sum"。
    // common 保留 C++ 访问路径 client->common.version()，但远端方法名是 "version"。
    constexpr static auto value =
        Object("calc", &::AppApi::calc,
               "common", make_tags<rpc_no_prefix_tag>(&::AppApi::common));
};
} // namespace NekoProto

int main() {
    ilias::PlatformContext context;
    RpcServer<JsonRpcBackend, AppApi> server{context};

    server->calc.add = [](int lhs, int rhs) -> ilias::IoTask<int> {
        co_return lhs + rhs;
    };
    server->calc.sum = [](std::vector<int> items) -> ilias::IoTask<int> {
        co_return std::accumulate(items.begin(), items.end(), 0);
    };
    server->common.version = []() -> ilias::IoTask<std::string> {
        co_return "1.0";
    };

    auto response = server.callMethod(
        R"({"jsonrpc":"2.0","method":"calc.add","params":[2,3],"id":1})"
    ).wait();
    std::string_view json{response.data(), response.size()};
}
```

常用调用方式：

*   `server->method = func`：绑定协议集合里声明的静态方法。
*   `server.bindMethod("name", func)` / `server.bindMethod<func>()`：动态注册额外方法。
*   `client->calc.add(1, 2)`：通过协议集合的 C++ 路径调用远端方法。
*   `client.callRemote<int>("calc.add", 1, 2)`：按远端方法名动态调用。
*   `client->rpc.getMethodList()`：调用默认内建 introspection 方法。
*   `client.notifyRemote<void>("name", args...)` 或 `client->method.notification(args...)`：发送 notification。
*   `server.processMessage(bytes)` / `server.callMethod(json)`：无连接地处理单条完整消息，适合测试、stdio、pipe 或自定义调度。

server/client 都默认带一个 `rpc` 成员，用于内建 introspection。它们的远端方法名使用 `rpc.` 前缀：

*   `rpc.get_method_list`: 获取当前服务端所有方法列表。
*   `rpc.get_bind_method_list`: 获取当前服务端所有已绑定方法列表。
*   `rpc.get_method_info`: 获取指定方法的描述。
*   `rpc.get_method_info_list`: 获取所有方法的描述。

```cpp
auto methods = client->rpc.getMethodList().wait();
auto info = client->rpc.getMethodInfo("calc.add").wait();
```

RPC 命名空间由协议集合的反射字段展开得到：

```cpp
struct AdminApi {
    RpcMethod<void(), "reload"> reload;
};

struct UserApi {
    RpcMethod<std::string(std::uint64_t), "name", "id"> name;
};

struct Api {
    AdminApi admin;
    UserApi user;
};

namespace NekoProto {
template <>
struct Meta<::AdminApi> {
    constexpr static auto value = Object("reload", &::AdminApi::reload);
};

template <>
struct Meta<::UserApi> {
    constexpr static auto value = Object("name", &::UserApi::name);
};

template <>
struct Meta<::Api> {
    constexpr static auto value =
        Object("admin", &::Api::admin,                                      // 远端名 "admin.reload"
               "user", make_tags<rpc_prefix_tag<"account">>(&::Api::user)); // 远端名 "account.name"
};
} // namespace NekoProto
```

`rpc_prefix_tag<"...">` 和 `rpc_no_prefix_tag` 只影响远端完整方法名，不改变 C++ 成员访问路径。完整方法名冲突会在注册期暴露，应优先通过显式前缀避免。

RPC 前端只负责方法元数据、注册、绑定、调用和调度，不负责监听、握手、鉴权或 session 管理。传输入口分两层：

*   `RpcMessageEndpoint`：已经能收发“完整 RPC message”的端点，适合 datagram、WebSocket message、stdio/LSP adapter 或外部协议已经完成分帧的场景。
*   `ilias::Stream`：只表示可读写字节流。字节流如何切成 RPC message 属于具体 backend 的协议内容，只有 backend 提供 `makeEndpoint(stream)` 时，`RpcServer` / `RpcClient` 才接受该 stream。

自定义完整消息 endpoint 满足下面的 concept 即可：

```cpp
template <typename T>
concept RpcMessageEndpoint = requires(T endpoint,
                                      std::vector<std::byte>& out,
                                      std::span<const std::byte> in) {
    { endpoint.recv(out) } -> std::same_as<ilias::IoTask<void>>;
    { endpoint.send(in) } -> std::same_as<ilias::IoTask<void>>;
    { endpoint.close() } -> std::same_as<void>;
    { endpoint.shutdown() } -> std::same_as<ilias::IoTask<void>>;
    { endpoint.flush() } -> std::same_as<ilias::IoTask<void>>;
};
```

对 stream-like 传输，backend 可提供非侵入 hook：

```cpp
struct MyRpcBackend {
    template <ilias::Stream StreamT>
    static auto makeEndpoint(StreamT stream); // 返回满足 RpcMessageEndpoint 的对象
};
```

`BinaryRpcBackend` 的 stream endpoint 直接读取 Neko-RPC 固定 header，并根据 header 中的 method/extension/payload size 读出完整 frame；不会再额外套一层通用 length-prefix。`JsonRpcBackend` 的 stream endpoint 则由 JSON-RPC backend 自己决定如何从字节流切 JSON-RPC message。当前库内测试使用轻量的内存双向流 `ilias::DuplexStream`，不用绑定 socket：

```cpp
#include <ilias/io/duplex.hpp>

auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);

RpcServer<BinaryRpcBackend, AppApi> server{context};
RpcClient<BinaryRpcBackend, AppApi> client{context};

server.addEndpoint(std::move(serverStream));
client.setEndpoint(std::move(clientStream));
```

Ilias 也提供 `ilias::PipePair`，但它是单向 OS pipe；做双向 RPC 时需要两对 pipe 或自行组合读端/写端。单元测试里优先使用 `DuplexStream`，更轻也不会占用端口。需要连接外部 JSON-RPC 服务/客户端时，应提供匹配对端 framing 的 JSON-RPC endpoint/backend hook，例如 LSP 风格 `Content-Length`、newline-delimited JSON 或 WebSocket message，而不是在 RPC frontend 固定一种 stream 分帧。

### 5.7. 自定义 RPC 后端扩展

RPC 后端的扩展方式和序列化扩展保持同一原则：不继承基类，不修改 `RpcMethod`，只通过模板使用点约束后端能力。`RpcDispatcher` 与 `RpcClient` 当前需要后端提供下列最小能力；具体 request/response wire shape、参数视图、id 类型和错误映射都由后端自己定义。

```cpp
struct MyRpcBackend {
    using Id = /* request id type */;
    using Message = std::vector<char>;
    using ResponseValues = /* backend-local response accumulator */;

    struct DecodedRequest {
        /* method name, id, params view, raw request value ... */
    };

    struct DecodeResult {
        bool ok = false;
        bool batch = false;
        std::vector<DecodedRequest> requests;
    };

    struct EncodedRequest {
        Message message;
        Id id;
    };

    static DecodeResult decodeIncoming(std::span<const std::byte> message);
    static std::string_view methodName(const DecodedRequest& request) noexcept;
    static const Id& id(const DecodedRequest& request) noexcept;
    static bool expectsResponse(const DecodedRequest& request) noexcept;

    template <typename Method>
    static ilias::Result</* decoded params tuple */, std::error_code>
    decodeParams(const DecodedRequest& request);

    template <typename Method>
    static ilias::IoTask<typename Method::RawReturnType>
    invoke(Method& method, /* decoded params tuple */ params);

    template <typename Method>
    static void appendMethodReturn(
        ResponseValues& responses,
        const DecodedRequest& request,
        ilias::Result<typename Method::RawReturnType, std::error_code> result
    );

    static void appendError(ResponseValues& responses,
                            const DecodedRequest& request,
                            std::error_code error);
    static Message encodeResponses(const ResponseValues& responses, bool batch);

    template <typename Method, typename... Args>
    static ilias::Result<EncodedRequest, std::error_code>
    encodeRequest(Method& method, bool notification, std::uint64_t& nextId, Args&&... args);

    template <typename Method>
    static ilias::Result<typename Method::RawReturnType, std::error_code>
    decodeResponse(std::span<const std::byte> message, const Id& expectedId);

    static std::error_code clientNotInitError();
    static std::error_code notificationOk();

    template <ilias::Stream StreamT>
    static auto makeEndpoint(StreamT stream); // 可选：返回 RpcMessageEndpoint
};
```

如果不需要人类可读的更轻巧的rpc, 可以使用本库提供的更小的 `NekoRpcBackend<Serializer, CodecId>`。它只保留 RPC 必需语义：请求、响应、通知、取消、错误、版本和扩展保留位；参数和返回值交给可替换 serializer policy。默认可直接使用 `BinaryRpcBackend`。

`Neko-RPC Core v1`：

```text
transport:
  RpcMessageEndpoint 处理完整 message。
  ilias::Stream 的分帧由 backend::makeEndpoint(stream) 定义；UDP/datagram 可直接把一个 datagram 当完整 message。

fixed header, big-endian:
  magic       u16   0x4e52        // "NR"
  version     u8    1
  kind        u8    1=request, 2=response, 3=notify, 4=cancel, 5=hello
  flags       u8    bit0=error, bit1=method_id, bit2=has_extensions, other bits reserved
  codec       u8    0=BinarySerializer, 1=JsonSerializer, 2=XmlSerializer, 128..255=user
  ext_size    u16   extension TLV bytes, 0 if none
  id          u64   request/response/cancel id, 0 for notify
  method_size u32   method bytes; response/cancel use 0
  payload_size u32  serialized params/result/error bytes

body:
  method bytes
  extension TLV bytes
  payload bytes
```

核心消息语义固定，不做“字段可省略”：

*   **request**: `kind=1`，必须有 `id`、`method`、`payload`；`payload` 是 `std::tuple<Args...>` 或后端定义的参数对象。
*   **response**: `kind=2`，必须有 `id`；成功时 `payload` 是返回值，`void` 返回空 payload；失败时设置 `flags.error`，`payload` 是固定错误对象。
*   **notify**: `kind=3`，必须有 `method`，`id=0`，服务端不返回 response。
*   **cancel**: `kind=4`，`id` 表示要取消的 request，不需要 method/payload。
*   **hello**: `kind=5`，可选，用于连接建立时协商 codec、压缩、最大帧长、method id 表等；协商信息放在 extension TLV，payload 为空，不要求每条消息携带协商信息。

extension TLV 建议固定为 `type: u16, size: u16, value: bytes`。`type` 的最高位可作为 critical 标记：不认识的非 critical TLV 可以跳过，不认识的 critical TLV 应拒绝消息或拒绝连接。

`method_id` 是 Neko-RPC backend 的 wire-level 可选扩展，属于后端协商出的传输优化，不进入通用 RPC 调用接口；初版实现通过 hello 同步连接期 full table，默认设计见 [`docs/rpc_method_id_design.md`](docs/rpc_method_id_design.md)。`rpc.get_method_list` / `rpc.get_method_info` 这些默认内建方法则是普通 RPC introspection，返回字符串方法名和描述；它们不改变 wire 格式，也不等同于已协商的 method id 表，最多只能作为构建或校验该表的输入。

错误对象保持最小固定形状：

```cpp
struct NekoRpcError {
    std::int32_t code = 0;
    std::string message;
    std::vector<std::byte> data;
};

namespace NekoProto {
template <>
struct Meta<::NekoRpcError> {
    constexpr static auto value =
        Object("code", &::NekoRpcError::code,
               "message", &::NekoRpcError::message,
               "data", &::NekoRpcError::data);
};
} // namespace NekoProto
```

`NekoRpcBackend` 是 serializer policy backend：

```cpp
template <typename Serializer>
struct NekoRpcBackend {
    using Message = std::vector<char>;
    using Id = std::uint64_t;

    // Header 固定用二进制大端读写，保证任意 codec 下都能先路由消息。
    // payload 由 Serializer::OutputSerializer / InputSerializer 读写。
};

using BinaryRpcBackend = NekoRpcBackend<BinarySerializer>;
```

使用方式和 JSON-RPC backend 一样，只替换 backend 类型：

```cpp
RpcServer<BinaryRpcBackend, AppApi> server{context};
RpcClient<BinaryRpcBackend, AppApi> client{context};

server->calc.add = [](int lhs, int rhs) -> ilias::IoTask<int> {
    co_return lhs + rhs;
};
```

这样做的取舍：

*   **比 JSON-RPC 简**：没有 JSON 对象字段名、`jsonrpc` 版本字段、1.0/2.0 兼容、`params` 省略、`id=null` 语义、batch 特例。
*   **比 gRPC/Cap'n Proto 轻**：不强制 IDL、代码生成、HTTP/2、streaming 生命周期或 capability 模型。
*   **比纯 MessagePack-RPC 更贴合本库**：wire header 固定，payload serializer 可替换；需要 MessagePack 互通时也可以新增 `MsgPackSerializer` 后端并用 `NekoRpcBackend<MsgPackSerializer, CodecId>` 承载。
*   **扩展可控**：扩展只走 `flags`、`kind` 保留值和 TLV；未知非关键 TLV 可跳过，关键 TLV 可拒绝连接或拒绝消息。
*   **性能路径清楚**：server 先读固定 header 得到 method/id/payload span，再按 method 的签名调用 serializer 解析参数。

建议先不把 batch 放进 core。多个请求可以连续发送多条 frame；真正需要批量时再增加 `kind=batch`，其 payload 是 frame 列表或 request 列表。这样第一版 backend 更小，取消、超时和错误映射也更直接。

设计检查结论：

*   **通用性**：RPC 前端只保留方法元数据、注册、绑定、调用和完整消息端点；JSON-RPC 的 id、batch、request/response 壳已下沉到 `JsonRpcBackend`。
*   **最小接口**：当前后端接口是按 `RpcDispatcher` / `RpcClient` 的实际使用点形成的最小能力集合，未额外引入 listener、session、transport 或继承层次；stream 支持通过可选 `makeEndpoint` 静态 hook 接入。
*   **非侵入扩展**：协议 struct 只需要 `RpcMethod` 字段和反射信息；命名策略通过 `make_tags<rpc_prefix_tag>` / `make_tags<rpc_no_prefix_tag>` 附着在字段使用点。
*   **和序列化扩展一致**：序列化类型用 `CustomParser<T>` 扩展，RPC 后端用静态函数和 concept 约束扩展；两者都避免要求业务类型继承框架基类。
*   **后续可改进点**：常见的 `invoke` / tuple 参数展开可以做成默认 helper，进一步缩小后端需要手写的代码。

---

## 6. 支持的类型

序列化后端会引入统一的 Parser 集合，常用 STL 容器和基础类型可直接使用，无需再包含单独的 `serialization/types/*.hpp`。0.3.0 后，JSON、Binary、XML 和 schema 生成会尽量复用同一套类型规则，只有格式确实无法表达的语义才由对应后端忽略或特化处理。

当前通用 Parser 覆盖的主要类型包括：算术类型、字符串、枚举、`std::optional`、指针、`std::variant`、`std::tuple`/`std::pair`、顺序容器、集合、映射、`std::array`、`std::bitset`、`std::atomic`、`std::byte`、`BinaryData<T>` 以及通过非侵入 `Meta<T>` 暴露反射信息的结构体。`NEKO_SERIALIZER` 仍作为便捷宏保留。

详细支持请参照 [Supported Types Overview](https://github.com/liuli-neko/NekoProtoTools/wiki/Supported-Types-Overview)。

## 7. 自定义序列化扩展

自定义 C++ 类型应特化 `CustomParser<T>`。读写逻辑可复用已有 parser 入口，因此同一扩展可以用于 JSON、Binary 和 XML 后端。`CustomParser<T>` 优先于内建反射和 STL parser。

```cpp
#include <nekoproto/serialization/parsing/parsers.hpp>

struct StrongId {
    std::uint64_t value = 0;
};

namespace NekoProto {
template <>
struct CustomParser<StrongId> {
    template <typename W, typename Parent, typename Tags>
    static ParserResult write(W& writer, const StrongId& id, const Parent& parent, const Tags& tags) {
        return parser_write<W>(writer, id.value, parent, tags);
    }

    template <typename R, typename Tags>
    static ParserResult read(typename R::InputValueType in, StrongId& id, const Tags& tags) {
        return parser_read<R>(in, id.value, tags);
    }

    static parsing::schema::Type toSchema() {
        return parser_schema<std::uint64_t>();
    }
};
} // namespace NekoProto
```

如果要增加一种新的数据格式，需实现与现有 `rapid::Reader/Writer`、`binary::Reader/Writer` 相同职责的后端，并由序列化器入口调用 `parser_write<Writer>` / `parser_read<Reader>`。

---

## 8. TODO

**序列化器 (Serializer)**

*   [x] 支持通过字符串名称访问协议字段 (基础反射)
*   [x] 使用 simdjson 作为 JSON 输入序列化器后端 (`simdjson::dom`)
*   [x] RapidJSON、simdjson、Binary、XML、schema 统一到通用 Parser 层
*   [x] 使用 pugixml 实现 XML Reader/Writer 后端
*   [x] 支持 Draft-07 风格 JSON Schema 生成
*   [x] `sa::Result<T>` 在 C++23 `<expected>` 可用时别名到 `std::expected<T, sa::Error>`，C++20 继续使用兼容实现
*   [ ] 支持 `simdjson::ondemand` 接口 (探索其与 `dom` 接口的性能和使用场景差异)
*   [x] 实现基于共享 JSON 文本 Writer 的 simdjson 输出路径
*   [x] 支持更多 C++ STL 容器

**ArgParser**

*   [x] 新增基于静态反射的命令行参数解析模块。
*   [x] 支持长/短选项、位置参数、flag、repeatable、嵌套选项、默认值、环境变量、choices、range、help/version。
*   [x] 支持子命令、别名、隐式值、分组、互斥关系、依赖关系、deprecated 提示和大小写不敏感 choices。

**通信 (Communication)**

*   [x] 支持 UDP 通信通道 (`ProtoDatagramClient`)
*   [ ] 支持更多底层传输协议 (如 WebSocket, QUIC - 可能通过 Ilias 或其他库集成)
*   [ ] 优化通信层原子性：确保数据帧的完整处理，即使在取消操作时也能保证数据流状态一致。可能需要调整为小帧发送机制。

**RPC**

*   [x] 支持 JSON-RPC 2.0 协议规范。
*   [x] 兼容 JSON-RPC 1.0 协议。
*   [x] RPC 前端与 JSON-RPC 后端分离，设计文档见 [`docs/rpc_refactor_plan.md`](docs/rpc_refactor_plan.md)。
*   [ ] 为外部 JSON-RPC 互通补充明确的 framing adapter，例如 LSP `Content-Length`、newline-delimited JSON、WebSocket message、datagram。
*   [x] 增加 `NekoRpcBackend<Serializer>` / `BinaryRpcBackend`，提供固定二进制帧头和可替换 payload serializer。
*   [x] 为 `NekoRpcBackend` 补充初版 hello 协商和连接期 full-table method id 优化，设计见 [`docs/rpc_method_id_design.md`](docs/rpc_method_id_design.md)。
*   [x] 完善 `NekoRpcBackend` 的动态 method id 表更新、delta/signature/兼容错误模型、压缩 TLV、可替换压缩 codec policy 和基础压缩统计。
*   [x] 增加显式 `RpcBackend` / `BackendSerializable` concept，改善自定义后端诊断。
*   [ ] JSON-RPC 扩展。
*   [x] 新增默认 `rpc` introspection 成员：
    - `rpc.get_method_list`: 获取当前服务端所有方法列表
    - `rpc.get_bind_method_list`: 获取当前服务端所有绑定方法列表
    - `rpc.get_method_info`: 获取指定方法信息
    - `rpc.get_method_info_list`: 获取所有方法信息列表
*   [x] 新增命名参数支持, 允许在声明或绑定方法时显示指定参数名称, 指定了名称的方法在调用时会以JsonObject的方式传递参数, 否则使用JsonArray的方式按位置传递。

---

## 9. 开发历史 (部分里程碑)
*   **v0.3.1**
    *   拆分序列化 parser 分发：写入走 `WriteParser<Writer, T>`，读取走 `ReadParser<Reader, T>`，schema 走 `SchemaParser<T>`；用户扩展点改为公开的 `CustomParser<T>`。
    *   新增 `NekoArgParser`/`<nekoproto/argparser/argparser.hpp>`：基于静态反射和 tags 定义 CLI schema，解析结果使用 `std::expected<..., std::error_code>`。
    *   手写反射元数据的推荐入口调整为非侵入 `template<> struct Meta<T>`；`NEKO_SERIALIZER` 继续作为便捷宏保留。
    *   ArgParser 支持长/短选项、位置参数、flag、repeatable、嵌套选项、默认值、环境变量、choices/range 校验、自动 help/version 和子命令。
    *   扩展 arg tags：`arg_aliases`、`arg_implicit`、`arg_group`、`arg_conflicts`、`arg_requires`、`arg_deprecated`、`arg_case_insensitive_choices` 等。
    *   重构 tags 基础设施，将通用 reflection tags/tag query 能力集中到 `global/reflection_tags.hpp`，供 serialization、RPC 和 argparser 共用。
    *   进一步分离 `NekoProtoBase` 与 `NekoCommunication` 的构建边界；通信模块继续作为可选能力。
    *   `sa::Result<T>` 在标准库支持 `<expected>` 时直接别名到 `std::expected<T, sa::Error>`，保留 C++20 fallback，并新增短名 `sa::Err(...)` 构造失败结果。

*   **v0.3.0**
    *   序列化后端重构：RapidJSON、simdjson、Binary、XML 和 schema 生成统一接入 `Parser<Reader, Writer, T>`。
    *   后端收敛为薄 Reader/Writer；旧 public 状态机 API 不再作为新后端扩展入口。
    *   反射 tags 从 value wrapper 迁移为独立字段/调用点元数据，删除旧 `TaggedValue` 用法。
    *   `BinaryTag::fixed_length`、`BinaryTag::unframed` 由实际消费层解释；`unframed` 不再是类型级 schema metadata。
    *   XML 切换为 pugixml 后端，支持对象字段、数组同名兄弟元素、节点文本和注释。
    *   通信层帧头 `MessageHeader` 不再注册进 `ProtoFactory`，由通信层显式以调用点 tag 读写。
    *   RPC 前端与 JSON-RPC 后端分离：`RpcServer<Backend, ProtocolSets...>` / `RpcClient<Backend, ProtocolSets...>` 成为通用入口，`JsonRpcServer` / `JsonRpcClient` 保留为 JSON-RPC 后端别名。
    *   新增 `NekoRpcBackend<Serializer>` / `BinaryRpcBackend`，提供精简二进制 RPC 帧头和可替换 payload serializer。

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
    *   给类型增加跳过标记is_skippable。标识一个非optional的可跳过类型。
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

本项目使用 MIT License，详见 [LICENSE](LICENSE)。
