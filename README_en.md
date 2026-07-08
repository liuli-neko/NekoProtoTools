# NekoProtoTools: A C++ Protocol Helper Library

[![中文](https://img.shields.io/badge/语言-中文-blue.svg)](./README.md)
[![English](https://img.shields.io/badge/language-English-blue.svg)](./README_en.md)
![Version](https://img.shields.io/badge/version-0.3.1-green.svg)

### CI Status

| Build Platform | Status | Code Coverage |
| :----------- | :----- | :------------ |
| Linux        | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) | [![codecov](https://codecov.io/gh/liuli-neko/NekoProtoTools/graph/badge.svg?token=F5OR647TV7)](https://codecov.io/gh/liuli-neko/NekoProtoTools) |
| Windows      | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml) | |
| Clang-cl | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml) | | 

---

## 1. Introduction

NekoProtoTools is a pure C++ protocol helper library designed to **simplify the definition, serialization/deserialization, and RPC communication of messages (protocols) in C++**.

Current version: **0.3.1**. Compared with v0.3.0, this release adds the reflection-driven ArgParser, YAML/TOML serialization backends, unified tag/reflection metadata infrastructure, and `NekoRpcBackend` hello negotiation, dynamic method-id table updates, bounded method-id error recovery, and compression extensions. On the serialization side, `sa::Result<T>` aliases `std::expected<T, sa::Error>` when `<expected>` is available, with the built-in compatibility implementation kept for older environments.

Core features of this library:

*   **Simplified Message Definition**: Business types only need to define fields. The recommended manual metadata entry is a non-intrusive `template<> struct Meta<T>`, which makes custom C++ types usable as serializable objects or protocol messages.
*   **Unified Parser Serialization Layer**: JSON, Binary, XML, YAML, TOML, and schema generation share one set of type rules; concrete formats provide only Reader/Writer implementations and small format-specific capabilities.
*   **Field-Level Metadata Tags**: `make_tags<Tag>(field)` describes a field or call site instead of permanently annotating the type, so the same type can use different layouts in different contexts.
*   **Reflection-Driven ArgParser**: Define CLI options with reflected objects and tags, including nested options, subcommands, defaults, environment variables, aliases, conflicts/requires rules, and help/version output.
*   **Basic Static Reflection Capability**: Provides a simple static reflection mechanism for compile-time type checks and metadata extraction.
*   **Generic RPC Frontend With Replaceable Backends**: RPC method declarations, registration, and calls are not tied to a specific wire protocol. Built-in backends currently include JSON-RPC and the compact binary `BinaryRpcBackend`.
*   **Type Safety**: Utilizes C++ template metaprogramming for compile-time type checking.

**Why This Library?**: Developers should focus their energy on business logic itself, rather than the tedious and error-prone construction and maintenance of protocols. This library aims to provide a stable, testable infrastructure to reduce the complexity of protocol handling.

---

## 2. Dependencies

*   **C++ Standard**:
    *   C++23 is recommended. The current xmake default is `stdcxx=23`, with C++20/C++26 configuration entries kept available.
*   **Serialization Backends**:
    *   JSON: [RapidJSON](https://rapidjson.org/) or [simdjson](https://simdjson.org/) (optional, enable via build options). When both are enabled, the default `JsonSerializer` alias selects RapidJSON first.
    *   XML: [pugixml](https://pugixml.org/) (optional, enable via build options)
    *   Binary: built-in binary Reader/Writer, no extra third-party dependency required.
    *   YAML: [libfyaml](https://github.com/pantoniou/libfyaml) (optional, enable via build options)
    *   TOML: [toml++](https://github.com/marzer/tomlplusplus) (optional, enable via build options)
    *   *Note: To keep the library lightweight, these serialization libraries are **not directly bundled**. You need to manage these dependencies yourself through options.*
*   **Communication & RPC (Optional)**:
    *   [Ilias](https://github.com/BusyStudent/ilias) (Used for network communication and asynchronous tasks/coroutines)
*   **log (Optional)**:
    *   [format](https://en.cppreference.com/w/cpp/utility/format/format) (If std::format is available in the compilation environment, you can enable logging directly)
    *   [fmt](https://fmt.dev/) (Use fmt as the logging backend)
    *   [spdlog](https://github.com/gabime/spdlog) (Use spdlog as the logging backend)
*   **Command-Line Argument Parsing**:
    *   ArgParser is header-only and has no extra third-party dependency. Include `<nekoproto/argparser/argparser.hpp>` to use it.

---

## 3. Installation & Integration (Example)

This library recommends using [xmake](https://xmake.io/) for building and integration.

Add the dependency in your `xmake.lua`:

```lua
-- xmake.lua
add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")

add_requires("neko-proto-tools", {
    configs = {
         enable_simdjson = false,    -- Set to true to enable simdjson backend
         enable_rapidjson = true,    -- Set to true to enable RapidJSON backend
         enable_pugixml = false,     -- Set to true to enable XML support
         enable_fmt = true,          -- Set to true if fmtlib support is needed (e.g., for logging)
         enable_protocol = true,     -- Set to true to enable ProtoFactory/IProto
         enable_communication = true,-- Set to true to enable communication features
         enable_jsonrpc = true,      -- Set to true to enable JSON-RPC features
         enable_libfyaml = false,    -- Set to true to enable YAML support
         enable_tomlplusplus = false -- Set to true to enable TOML support
    },
})

target("your_project")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("neko-proto-tools")
    -- Also add dependencies based on the enabled options if needed by your project directly
    -- e.g., add_packages("ilias") if using communication/rpc
```

---

## 4. Quick Start

### 4.1. Basic Serialization/Deserialization

Only include the basic headers and provide non-intrusive `Meta<T>` metadata for the type. `NEKO_SERIALIZER` remains available for simple cases, but the README promotes `Meta<T>` so business types and library metadata stay separate.

```cpp
#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/serializer_base.hpp>
#include <nekoproto/serialization/json_serializer.hpp> // Use JSON serializer
#include <iostream>
#include <string>
#include <vector>

// Define your data structure
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
    using namespace NekoProto; // Introduce namespace

    MyData data_out;
    data_out.id = 101;
    data_out.name = "Neko";
    data_out.score = 99.5;

    // --- Serialization ---
    std::vector<char> buffer;
    JsonSerializer::OutputSerializer serializer(buffer); // Create JSON output serializer
    serializer(data_out); // Serialize the data_out object
    serializer.end();     // End serialization, flush buffer (can also be done automatically via destructor)

    std::string json_str(buffer.begin(), buffer.end());
    std::cout << "Serialized JSON: " << json_str << std::endl;
    // Output: Serialized JSON: {"id":101,"name":"Neko","score":99.5}

    // --- Deserialization ---
    MyData data_in;
    // Create JSON input serializer from buffer data and size
    JsonSerializer::InputSerializer deserializer(buffer.data(), buffer.size());
    // Deserialize JSON data from buffer into data_in
    if (deserializer(data_in)) { // Check if deserialization was successful
        std::cout << "Deserialized Data: id=" << data_in.id
                  << ", name=" << data_in.name
                  << ", score=" << data_in.score << std::endl;
        // Output: Deserialized Data: id=101, name=Neko, score=99.5
    } else {
        std::cerr << "Deserialization failed!" << std::endl;
    }

    return 0;
}
```

### 4.2. Defining Protocol Messages (with Reflection and Polymorphism)

If you need protocol management, reflection, and polymorphism support, include `proto_base.hpp` and use `NEKO_DECLARE_PROTOCOL` for the protocol wrapper entry. Field metadata is still recommended as external `Meta<T>` metadata.

```cpp
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/serializer_base.hpp>
#include <nekoproto/serialization/json_serializer.hpp> // Specify default serializer
#include <iostream>
#include <string>
#include <vector>

// Define protocol message struct
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

    // --- Use the factory to create and manage protocols ---
    // Specify protocol version when creating the factory (major, minor, patch)
    ProtoFactory factory(1, 0, 0);

    // 1. Create UserProfile instance (via protocol base class interface)
    // emplaceProto returns an IProto interface pointer wrapping the specific protocol object
    // You can pass constructor arguments directly to emplaceProto
    auto user_iproto = UserProfile::emplaceProto(123, "Alice");

    // Serialize to byte data (using the protocol's specified default serializer: JsonSerializer)
    std::vector<char> user_data = user_iproto.toData();
    std::cout << "UserProfile Serialized Size: " << user_data.size() << " bytes" << std::endl;

    // 2. Create protocol instance by name via factory
    IProto login_iproto = factory.create("LoginRequest"); // Returns IProto interface
    if (!login_iproto) {
         std::cerr << "Failed to create LoginRequest instance!" << std::endl;
         return 1;
    }

    // Safely cast to specific type pointer using cast<T>() (returns nullptr if type doesn't match)
    auto login_req = login_iproto.cast<LoginRequest>();
    if (login_req) {
        login_req->username = "Bob";
        login_req->password_hash = "some_hash_value";
    }

    std::vector<char> login_data = login_iproto.toData();
    std::cout << "LoginRequest Serialized Size: " << login_data.size() << " bytes" << std::endl;


    // 3. Deserialize from data (assuming we received UserProfile data)
    IProto received_proto = factory.create("UserProfile"); // First create an empty instance of the corresponding type
    if (received_proto.fromData(user_data)) { // Fill from data
        std::cout << "Successfully deserialized data into " << received_proto.name() << std::endl;
        auto received_user = received_proto.cast<UserProfile>();
        if (received_user) {
            std::cout << "Received User ID: " << received_user->userId << std::endl;
            // Output: Received User ID: 123
        }
    } else {
        std::cerr << "Failed to deserialize user data." << std::endl;
    }

    return 0;
}
```

---

## 5. Core Concepts Explanation

### 5.1. Serializer (`Serializer`)

Serializers are responsible for converting C++ objects into byte streams (serialization) and converting byte streams back into C++ objects (deserialization). Since 0.3.0, type dispatch lives in the generic `Parser<Reader, Writer, T>` layer; concrete formats provide Reader/Writer implementations plus optional capabilities.

*   **Built-in Serializers**:
    *   `JsonSerializer`: Default JSON serializer alias. It uses `RapidJsonSerializer` when RapidJSON is enabled; otherwise it uses `SimdJsonSerializer` when simdjson is enabled.
    *   `BinarySerializer`: Provides a compact binary serialization format and supports binary layout tags such as `fixed_length` and `unframed`.
    *   `XmlSerializer`: Uses pugixml for XML serialization and deserialization.
    *   `YamlSerializer`: Uses libfyaml for YAML serialization and deserialization.
    *   `TomlSerializer`: Uses toml++ for TOML serialization and deserialization.
*   **Choosing a Serializer**:
    *   For basic serialization, directly instantiate the required `OutputSerializer` / `InputSerializer`.
    *   For protocol messages (`NEKO_DECLARE_PROTOCOL`), specify the default serializer during declaration.
*   **Schema Generation**: Include `<nekoproto/serialization/json/schema.hpp>` and call `generate_schema<T>(schema)` to generate a Draft-07 style JSON Schema.
*   **Serialization Extensions**: Use `CustomParser<T>` for custom types; new formats implement Reader/Writer backends. See [7. Serialization Extensions](#7-serialization-extensions).

### 5.2. Field Tags And Call-Site Metadata

`make_tags<Tag>(value_or_accessor)` describes how a field should be handled in the current binding. It is not permanent type metadata, so the same type can use different tags in different structs, backends, or top-level calls.

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

Common built-in tags:

*   `JsonTag{.flat = true}`: flatten reflected object fields into the parent object.
*   `JsonTag{.skippable = true}`: allow missing fields when reading.
*   `JsonTag{.raw_string = true}`: treat a JSON string as a raw JSON fragment.
*   `BinaryTag{.fixed_length = N}`: read/write binary scalar values with a fixed width.
*   `BinaryTag{.unframed = true}`: read/write reflected binary objects without field boundaries; use it at the call site for things such as transport headers, not as type-level metadata.
*   `rename_tag<"...">` / `comment_tag<"...">`: reflection metadata for field renaming and XML comments.

### 5.3. Command-Line Argument Parsing (`ArgParser`)

ArgParser reuses the static reflection and tag system to map a plain options struct into a command-line schema. Parsing returns `std::expected<T, std::error_code>`; `--help`, `--version`, missing required options, and invalid values are reported through `ArgParserError`.

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

Common arg tags include `arg_name`, `arg_help`, `arg_default`, `arg_choices`, `arg_env`, `arg_separator`, `arg_aliases`, `arg_implicit`, `arg_group`, `arg_conflicts`, `arg_requires`, `arg_deprecated`, and `arg_case_insensitive_choices`. Base behavior is described with `ArgTags{.positional = true}`, `.flag = true`, `.repeatable = true`, `.required = true`, and related fields; subcommands are modeled with `ArgTags{.command = true}`.

### 5.4. Protocol Management (`ProtoFactory`, `IProto`)

Use the protocol management mechanism when you need to manage different types of protocols, handle polymorphism, or require runtime type information.

*   **`NEKO_DECLARE_PROTOCOL(ClassName, DefaultSerializer)`**:
    *   Registers a class as a protocol.
    *   Allows it to use the `IProto` interface methods.
    *   Automatically registers it with `ProtoFactory`, allowing creation by name or type ID.
    *   Specifies the default serializer for this protocol.
*   **`ProtoFactory`**:
    *   A protocol factory class responsible for creating protocol instances (`IProto`) based on protocol name or type ID.
    *   Manages protocol version information.
    *   Automatically registers all protocols declared with `NEKO_DECLARE_PROTOCOL` (in alphabetical order).
*   **`IProto`**:
    *   An abstract base class interface for protocols.
    *   Provides common methods like `toData()` (serialize), `fromData()` (deserialize), `name()`, `type()`, etc.
    *   Use `emplaceProto()` to create instances, returning `IProto`.
    *   Use `cast<T>()` to safely cast an `IProto` back to a specific protocol type pointer.

### 5.5. Communication (`Communication`)

This library provides a coroutine-based communication abstraction layer built upon [Ilias](https://github.com/BusyStudent/ilias), designed for conveniently transmitting protocol messages over network connections.

*   **Required Header**: `#include <nekoproto/communication/communication_base.hpp>`
*   **Core Class**: `ProtoStreamClient<UnderlyingStream>` (e.g., `ProtoStreamClient<TcpClient>`)
*   **Functionality**: Wraps underlying network streams (like TCP, UDP), automatically handling the packing (adding type info and length), sending, receiving, and unpacking of protocol messages.

**Example (TCP Communication)**:

> Full example code: `tests/unit/communication/test_communication.cpp`

```cpp
#include <nekoproto/communication/communication_base.hpp>
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/json_serializer.hpp>

#include <ilias/net.hpp>         // Ilias network library
#include <ilias/platform.hpp>  // Ilias platform context
#include <ilias/task.hpp>        // Ilias coroutine/task library

#include <iostream>
#include <vector>
#include <string>
#include <chrono> // For timestamp

NEKO_USE_NAMESPACE // Use nekoproto namespace
using namespace ilias; // Use ilias namespace

// Define the message protocol to be transmitted
class ChatMessage {
public:
    uint64_t    timestamp;
    std::string sender;
    std::string content;

    NEKO_DECLARE_PROTOCOL(ChatMessage, JsonSerializer); // Declare as a protocol
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

// Simple server coroutine
ilias::Task<> server_task(PlatformContext& ioContext, ProtoFactory& protoFactory) {
    TcpListener listener(ioContext, AF_INET);
    if (!listener.bind(IPEndpoint("127.0.0.1", 12345))) {
        std::cerr << "Server failed to bind." << std::endl;
        co_return;
    }
    std::cout << "Server listening on 127.0.0.1:12345" << std::endl;

    auto accept_result = co_await listener.accept(); // Wait for connection
    if (!accept_result) {
        std::cerr << "Server accept failed: " << accept_result.error().message() << std::endl;
        co_return;
    }
    std::cout << "Client connected." << std::endl;

    // Wrap the TCP connection into a protocol stream client (server-side perspective)
    // accept_result.value() is likely a pair {TcpClient, IPEndpoint}
    ProtoStreamClient<TcpClient> proto_stream(protoFactory, ioContext, std::move(accept_result.value().first));

    // Receive a message
    auto recv_result = co_await proto_stream.recv();
    if (recv_result) {
        IProto received_proto = std::move(recv_result.value());
        std::cout << "Server received protocol: " << received_proto.name() << std::endl;
        auto msg = received_proto.cast<ChatMessage>();
        if (msg) {
            std::cout << "  Timestamp: " << msg->timestamp << std::endl;
            std::cout << "  Sender: " << msg->sender << std::endl;
            std::cout << "  Content: " << msg->content << std::endl;

            // Reply with a message
            ChatMessage reply_msg;
            reply_msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
            reply_msg.sender = "Server";
            reply_msg.content = "Message received!";
            auto send_reply_result = co_await proto_stream.send(reply_msg.makeProto()); // Send protocol
            if (send_reply_result) {
                 std::cout << "Server sent reply." << std::endl;
            } else {
                 std::cerr << "Server failed to send reply: " << send_reply_result.error().message() << std::endl;
            }
        }
    } else {
        std::cerr << "Server failed to receive message: " << recv_result.error().message() << std::endl;
    }

    proto_stream.close(); // Close connection
    listener.close();
    std::cout << "Server task finished." << std::endl;
}

// Simple client coroutine
ilias::Task<> client_task(PlatformContext& ioContext, ProtoFactory& protoFactory) {
    TcpClient tcpClient(ioContext, AF_INET);
    auto conn_result = co_await tcpClient.connect(IPEndpoint("127.0.0.1", 12345)); // Connect to server
    if (!conn_result) {
        std::cerr << "Client failed to connect: " << conn_result.error().message() << std::endl;
        co_return;
    }
    std::cout << "Client connected to server." << std::endl;

    // Wrap the TCP connection into a protocol stream client
    ProtoStreamClient<TcpClient> proto_stream(protoFactory, ioContext, std::move(tcpClient));

    // Prepare the message to send
    ChatMessage msg_to_send;
    msg_to_send.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::system_clock::now().time_since_epoch())
                                .count();
    msg_to_send.sender = "Client";
    msg_to_send.content = "Hello from client!";

    // Send the message (makeProto() creates IProto instance)
    // SerializerInThread option indicates serialization occurs in another thread pool thread
    auto send_result = co_await proto_stream.send(msg_to_send.makeProto(), ProtoStreamClient<TcpClient>::SerializerInThread);
    if (!send_result) {
         std::cerr << "Client failed to send message: " << send_result.error().message() << std::endl;
         proto_stream.close();
         co_return;
    }
     std::cout << "Client sent message." << std::endl;

    // Receive the reply
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

    proto_stream.close(); // Close connection
    std::cout << "Client task finished." << std::endl;
}

int main() {
    PlatformContext ioContext;    // Ilias coroutine context
    ProtoFactory protoFactory(1, 0, 0); // Protocol factory

    // Run server and client tasks using ilias_go (starts task in context)
    ilias_go server_task(ioContext, protoFactory);
    // Add a small delay for the server to start listening before client connects
    ilias_go ilias::sleep_for(std::chrono::milliseconds(100)).then([&]() {
        return client_task(ioContext, protoFactory);
    });


    ioContext.run(); // Run the Ilias event loop until all tasks complete

    return 0;
}
```

### 5.6. Generic RPC Frontend

The RPC module is now split into a generic frontend plus replaceable backends. `RpcMethod`, protocol sets, and the server/client calling API are no longer tied to JSON-RPC. JSON-RPC 2.0 is one built-in backend that handles the request/response envelope, ids, batch calls, notifications, parameter encoding, and error mapping.

*   **Generic Header**: `#include <nekoproto/rpc/rpc.hpp>`
*   **JSON-RPC Backend**: `#include <nekoproto/jsonrpc/backend.hpp>`, or include the compatibility aggregate header `#include <nekoproto/jsonrpc/jsonrpc.hpp>`
*   **Generic API**: `RpcServer<Backend, ProtocolSets...>` / `RpcClient<Backend, ProtocolSets...>`
*   **Convenience Aliases**: `JsonRpcServer<Api>` is equivalent to `RpcServer<JsonRpcBackend, Api>`, and `JsonRpcClient<Api>` is equivalent to `RpcClient<JsonRpcBackend, Api>`
*   **Async Support**: Method implementations can return plain values, `void`, or `ilias::IoTask<T>`. Client calls uniformly return `ilias::IoTask<T>`.

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
    // calc is registered as "calc.add" / "calc.sum".
    // common keeps the C++ access path client->common.version(),
    // while the remote method name is just "version".
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

Common call patterns:

*   `server->method = func`: Bind a statically declared method from a protocol set.
*   `server.bindMethod("name", func)` / `server.bindMethod<func>()`: Dynamically register an extra method.
*   `client->calc.add(1, 2)`: Call a remote method through the C++ protocol path.
*   `client.callRemote<int>("calc.add", 1, 2)`: Dynamically call by remote method name.
*   `client->rpc.getMethodList()`: Call the built-in introspection methods.
*   `client.notifyRemote<void>("name", args...)` or `client->method.notification(args...)`: Send a notification.
*   `server.processMessage(bytes)` / `server.callMethod(json)`: Process one complete message without owning a connection. This is useful for tests, stdio, pipes, or custom dispatch.

Servers and clients include a default `rpc` member for built-in introspection. Its remote method names use the `rpc.` prefix:

*   `rpc.get_method_list`: Get all methods on the current server.
*   `rpc.get_bind_method_list`: Get all currently bound methods on the server.
*   `rpc.get_method_info`: Get the description of one method.
*   `rpc.get_method_info_list`: Get descriptions for all methods.

```cpp
auto methods = client->rpc.getMethodList().wait();
auto info = client->rpc.getMethodInfo("calc.add").wait();
```

RPC namespaces are derived by expanding reflected fields in protocol sets:

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
        Object("admin", &::Api::admin,                                      // remote name "admin.reload"
               "user", make_tags<rpc_prefix_tag<"account">>(&::Api::user)); // remote name "account.name"
};
} // namespace NekoProto
```

`rpc_prefix_tag<"...">` and `rpc_no_prefix_tag` only affect the full remote method name; they do not change the C++ member access path. Full-name conflicts surface during registration, so prefer explicit prefixes when APIs may collide.

The RPC frontend only handles method metadata, registration, binding, calls, and dispatch. It does not own listening, handshakes, authentication, or session management. Transport entry points are split into two layers:

*   `RpcMessageEndpoint`: An endpoint that already sends and receives complete RPC messages. It fits datagrams, WebSocket messages, stdio/LSP adapters, or any outer protocol that already performs framing.
*   `ilias::Stream`: A byte stream. Turning bytes into complete RPC messages is backend-specific. `RpcServer` / `RpcClient` accept a stream only when the backend provides `makeEndpoint(stream)`.

A custom complete-message endpoint only needs to satisfy this concept:

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

For stream-like transports, a backend can provide a non-intrusive hook:

```cpp
struct MyRpcBackend {
    template <ilias::Stream StreamT>
    static auto makeEndpoint(StreamT stream); // returns an RpcMessageEndpoint
};
```

`BinaryRpcBackend` reads the fixed Neko-RPC header directly from the stream, then uses the method, extension, and payload sizes from the header to read the rest of the frame. It does not wrap frames in an additional generic length prefix. `JsonRpcBackend` owns its own stream framing rule. Unit tests use the lightweight in-memory `ilias::DuplexStream` instead of binding sockets:

```cpp
#include <ilias/io/duplex.hpp>

auto [clientStream, serverStream] = ilias::DuplexStream::make(65536);

RpcServer<BinaryRpcBackend, AppApi> server{context};
RpcClient<BinaryRpcBackend, AppApi> client{context};

server.addEndpoint(std::move(serverStream));
client.setEndpoint(std::move(clientStream));
```

Ilias also provides `ilias::PipePair`, but it is a one-way OS pipe. Bidirectional RPC over pipes needs two pipe pairs or a custom composed read/write stream. Tests prefer `DuplexStream` because it is lighter and does not consume ports. When interoperating with external JSON-RPC peers, provide a JSON-RPC endpoint/backend hook that matches the peer framing, such as LSP `Content-Length`, newline-delimited JSON, WebSocket messages, or datagrams, instead of fixing one stream framing in the RPC frontend.

### 5.7. Custom RPC Backend Extension

RPC backends follow the same extension principle as serializers: no base-class inheritance and no changes to `RpcMethod`; backend capabilities are consumed through template use sites. `RpcDispatcher` and `RpcClient` currently need the following minimal backend surface. The concrete request/response wire shape, parameter view, id type, and error mapping are backend-defined.

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
    decodeParams(const DecodedRequest& request, const Method& method);

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
    static auto makeEndpoint(StreamT stream); // optional: returns RpcMessageEndpoint
};
```

If you want a lighter non-human-readable RPC protocol, use the built-in `NekoRpcBackend<Serializer, CodecId>`. It keeps only the required RPC semantics: request, response, notification, cancellation, error, version, and reserved extension bits. Parameters and return values are handled by the replaceable serializer policy. The default alias is `BinaryRpcBackend`.

`Neko-RPC Core v1`:

```text
transport:
  RpcMessageEndpoint handles complete messages.
  ilias::Stream framing is defined by backend::makeEndpoint(stream).
  UDP/datagram transports can treat one datagram as one complete message.

fixed header, big-endian:
  magic        u16   0x4e52        // "NR"
  version      u8    1
  kind         u8    1=request, 2=response, 3=notify, 4=cancel, 5=hello
  flags        u8    bit0=error, bit1=method_id, bit2=has_extensions, other bits reserved
  codec        u8    0=BinarySerializer, 1=JsonSerializer, 2=XmlSerializer, 128..255=user
  ext_size     u16   extension TLV bytes, 0 if none
  id           u64   request/response/cancel id, 0 for notify
  method_size  u32   method bytes; response/cancel use 0
  payload_size u32   serialized params/result/error bytes

body:
  method bytes
  extension TLV bytes
  payload bytes
```

Core message semantics are fixed and do not use omitted fields:

*   **request**: `kind=1`, must include `id`, `method`, and `payload`. The payload is `std::tuple<Args...>` or a backend-defined parameter object.
*   **response**: `kind=2`, must include `id`. Success payload is the return value; `void` returns an empty payload. Failure sets `flags.error` and uses a fixed error object as the payload.
*   **notify**: `kind=3`, must include `method`, uses `id=0`, and does not receive a server response.
*   **cancel**: `kind=4`, uses `id` to identify the request to cancel and does not need method/payload.
*   **hello**: `kind=5`, optional. It can negotiate codec, compression, maximum frame size, method id tables, and similar connection-level features through extension TLVs. Payload is empty.

Extension TLV is recommended as `type: u16, size: u16, value: bytes`. The highest bit of `type` can mark a critical extension: unknown non-critical TLVs may be skipped, while unknown critical TLVs should reject the message or connection.

`method_id` is an optional Neko-RPC backend wire-level extension. It is a backend-negotiated transport optimization and does not enter the generic RPC calling API. The default implementation can attach a method table to hello or method-id error responses when it fits the configured budget; the client-side backend policy decides whether to apply that table and retry. The design lives in [`docs/rpc_method_id_design.md`](docs/rpc_method_id_design.md). The default built-in methods such as `rpc.get_method_list` and `rpc.get_method_info` are normal RPC introspection calls that return string method names and descriptions. They do not change the wire format and are not the negotiated method id table, although they can be used as input when building or validating one.

The error object stays minimal:

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

`NekoRpcBackend` is a serializer-policy backend:

```cpp
template <typename Serializer>
struct NekoRpcBackend {
    using Message = std::vector<char>;
    using Id = std::uint64_t;

    // Header read/write is fixed big-endian binary so messages can be routed
    // before the payload codec is known.
    // Payload is written/read by Serializer::OutputSerializer/InputSerializer.
};

using BinaryRpcBackend = NekoRpcBackend<BinarySerializer>;
```

Usage is the same as JSON-RPC, with only the backend type changed:

```cpp
RpcServer<BinaryRpcBackend, AppApi> server{context};
RpcClient<BinaryRpcBackend, AppApi> client{context};

server->calc.add = [](int lhs, int rhs) -> ilias::IoTask<int> {
    co_return lhs + rhs;
};
```

Tradeoffs:

*   **Simpler than JSON-RPC**: no JSON object field names, `jsonrpc` version field, 1.0/2.0 compatibility, omitted `params`, `id=null` semantics, or batch special cases.
*   **Lighter than gRPC/Cap'n Proto**: no required IDL, code generation, HTTP/2, streaming lifecycle, or capability model.
*   **Closer to this library than plain MessagePack-RPC**: the wire header is fixed and the payload serializer is replaceable. If MessagePack interoperability is needed, add a `MsgPackSerializer` backend and use `NekoRpcBackend<MsgPackSerializer, CodecId>`.
*   **Controlled extension surface**: extensions go through reserved `flags`, `kind` values, and TLV. Unknown non-critical TLVs can be skipped; critical TLVs can reject the connection or message.
*   **Clear performance path**: the server first reads a fixed header to obtain method/id/payload information, then parses parameters with the serializer selected by the method signature.

Batch is intentionally not part of the first core binary protocol. Multiple requests can be sent as consecutive frames. If batch is needed later, it can be added as a new `kind=batch` whose payload is a list of frames or requests. Keeping it out of v1 makes cancellation, timeout, and error mapping simpler.

Design check:

*   **Generality**: The RPC frontend keeps only method metadata, registration, binding, calls, and complete-message endpoints. JSON-RPC ids, batch handling, request/response envelopes, and wire error mapping live in `JsonRpcBackend`.
*   **Minimal Interface**: The current backend interface follows the actual use sites in `RpcDispatcher` / `RpcClient`; it does not add listeners, sessions, transport ownership, or inheritance layers. Stream support is attached through the optional static `makeEndpoint` hook.
*   **Non-Intrusive Extension**: Protocol structs only need `RpcMethod` fields and reflection metadata. Naming policy is attached at the field use site through `make_tags<rpc_prefix_tag>` / `make_tags<rpc_no_prefix_tag>`.
*   **Consistent With Serialization Extensions**: Serialization extends through `CustomParser<T>`, while RPC backends extend through static functions and concept-style requirements. Both avoid requiring business types to inherit framework base classes.
*   **Future Improvements**: Common `invoke` / tuple expansion helpers could reduce boilerplate for custom backends.

---

## 6. Supported Types

Serialization backends include the common Parser set, so standard containers and basic types no longer require separate `serialization/types/*.hpp` headers. JSON, Binary, XML, YAML, TOML, and schema generation now reuse the same type rules whenever possible; format-specific behavior is handled by backend capabilities or explicit specializations.

The common Parser set currently covers arithmetic types, strings, enums, `std::optional`, pointers, `std::variant`, `std::tuple`/`std::pair`, sequence containers, sets, maps, `std::array`, `std::bitset`, `std::atomic`, `std::byte`, `BinaryData<T>`, and reflected structs exposed through non-intrusive `Meta<T>` metadata. `NEKO_SERIALIZER` remains available as a convenience macro.

more details can be found in the [Supported Types Overview](https://github.com/liuli-neko/NekoProtoTools/wiki/Supported-Types-Overview).

---

## 7. Serialization Extensions

To support a custom C++ type, specialize the public `CustomParser<T>`. The implementation can reuse existing parser entries, so the same extension can work across JSON, Binary, XML, YAML, TOML, and other backends.

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

To add a new data format, implement a backend with responsibilities matching the existing `rapid::Reader/Writer` or `binary::Reader/Writer`, then call `parser_write` and `parser_read` from its serializer entry points.

---

## 8. TODO

**Serializer**

*   [x] Support accessing protocol fields by string name (basic reflection)
*   [x] Use simdjson as JSON input serializer backend (`simdjson::dom`)
*   [x] Route RapidJSON, simdjson, Binary, XML, YAML, TOML, and schema generation through the generic Parser layer
*   [x] Implement XML Reader/Writer backend with pugixml
*   [x] Implement YAML Reader/Writer backend with libfyaml
*   [x] Implement TOML Reader/Writer backend with toml++
*   [x] Support Draft-07 style JSON Schema generation
*   [x] Alias `sa::Result<T>` to `std::expected<T, sa::Error>` when C++23 `<expected>` is available, while keeping the C++20 compatibility implementation
*   [ ] Support `simdjson::ondemand` interface (Explore performance and use case differences with `dom`)
*   [x] Implement simdjson output through the shared JSON text Writer path
*   [x] Support more C++ STL containers

**ArgParser**

*   [x] Add a static-reflection-based command-line argument parser.
*   [x] Support long/short options, positionals, flags, repeatable values, nested options, defaults, environment variables, choices, ranges, help/version output.
*   [x] Support subcommands, aliases, implicit values, groups, conflicts, requires rules, deprecated warnings, and case-insensitive choices.

**Communication**

*   [x] Support UDP communication channel (`ProtoDatagramClient`)
*   [ ] Support more underlying transport protocols (e.g., WebSocket, QUIC - possibly via Ilias or other libraries)
*   [ ] Enhance communication layer atomicity: Ensure complete processing of data frames, maintaining consistent data stream state even during cancellation operations. May require adjusting to a small-frame sending mechanism.

**RPC**

*   [x] Support JSON-RPC 2.0 protocol specification.
*   [x] Compatibility with JSON-RPC 1.0 protocol (Optional).
*   [x] Split the RPC frontend from the JSON-RPC backend. See [`docs/rpc_refactor_plan.md`](docs/rpc_refactor_plan.md).
*   [ ] Add explicit framing adapters for external JSON-RPC interoperability, such as LSP `Content-Length`, newline-delimited JSON, WebSocket messages, and datagrams.
*   [x] Add `NekoRpcBackend<Serializer>` / `BinaryRpcBackend` with a fixed binary frame header and replaceable payload serializer.
*   [x] Add `NekoRpcBackend` hello negotiation, budgeted method table delivery, and method-id error recovery. See [`docs/rpc_method_id_design.md`](docs/rpc_method_id_design.md).
*   [x] Complete `NekoRpcBackend` dynamic method id table updates, delta/signature/compatibility error handling, compression TLVs, replaceable compression codec policy, and basic compression statistics.
*   [x] Add explicit `RpcBackend` / `BackendSerializable` concepts for clearer custom-backend diagnostics.
*   [ ] Support for JSON-RPC extensions.
*   [x] Add the default `rpc` introspection member:
    - `rpc.get_method_list`: Get a list of all methods on the current server
    - `rpc.get_bind_method_list`: Get a list of all bound methods on the current server
    - `rpc.get_method_info`: Get a description of a specified method
    - `rpc.get_method_info_list`: Get descriptions of all methods
*   [x] Support named parameters, allowing method declarations or bindings to specify parameter names. Methods with specified names pass parameters as JSON objects; others pass them as JSON arrays by position.

**Maintenance Direction (not tied to a specific next version yet)**

*   [ ] Continue simplifying internal code, tightening long flows and duplicated implementation, and fixing issues as they are found.
*   [ ] Improve ergonomics, diagnostics, and documentation examples so new projects are easier to integrate.
*   [ ] Add stress tests and performance evaluation benchmarks, including comparisons with similar libraries.

---

## 9. Development History (Selected Milestones)

*   **v0.3.1**
    *   Split serialization parser dispatch into `WriteParser<Writer, T>`, `ReadParser<Reader, T>`, and `SchemaParser<T>`; the user extension point is now the public `CustomParser<T>`.
    *   Added YAML / TOML serialization backends: `YamlSerializer` is based on libfyaml, `TomlSerializer` is based on toml++, and both are wired into the unified Parser type rules.
    *   Recommended manual reflection metadata now uses non-intrusive `template<> struct Meta<T>`; `NEKO_SERIALIZER` remains as a convenience macro.
    *   Added `NekoArgParser` / `<nekoproto/argparser/argparser.hpp>`: CLI schemas can now be defined with static reflection and tags, returning `std::expected<..., std::error_code>`.
    *   ArgParser supports long/short options, positional arguments, flags, repeatable values, nested options, defaults, environment variables, choices/ranges, automatic help/version output, and subcommands.
    *   Expanded arg tags with `arg_aliases`, `arg_implicit`, `arg_group`, `arg_conflicts`, `arg_requires`, `arg_deprecated`, `arg_case_insensitive_choices`, and related metadata.
    *   Refactored the tag infrastructure so common reflection tags and tag queries live in `global/reflection_tags.hpp` and are shared by serialization, RPC, and argparser.
    *   Completed the `NekoRpcBackend` connection-state model with hello negotiation, dynamic method-id table refresh, delta/signature/compatibility errors, budgeted method-table data on error responses, and one bounded client recovery retry.
    *   Added `NekoRpcBackend` compression TLVs, a replaceable compression codec policy, and basic compression statistics while keeping compression state inside the backend context/session.
    *   Further separated the build boundary between `NekoProtoBase` and `NekoCommunication`; communication remains an optional module.
    *   `sa::Result<T>` now aliases `std::expected<T, sa::Error>` when the standard library provides `<expected>`, keeps the C++20 fallback, and adds the short `sa::Err(...)` helper for failed results.

*   **v0.3.0**
    *   Reworked the serialization stack so RapidJSON, simdjson, Binary, XML, and schema generation share `Parser<Reader, Writer, T>`.
    *   Reduced concrete format backends to thin Reader/Writer layers; the old public state-machine API is no longer the extension model for new backends.
    *   Moved reflection tags from value wrappers to independent field/call-site metadata and removed the old `TaggedValue` style.
    *   `BinaryTag::fixed_length` and `BinaryTag::unframed` are interpreted by the layer that consumes them; `unframed` is no longer type-level schema metadata.
    *   Switched XML support to pugixml, including object fields, repeated sibling elements for arrays, node text, and comments.
    *   Stopped registering the communication `MessageHeader` as a `ProtoFactory` protocol; it is now serialized explicitly by the communication layer with call-site tags.
    *   Split the RPC frontend from the JSON-RPC backend: `RpcServer<Backend, ProtocolSets...>` / `RpcClient<Backend, ProtocolSets...>` are now the generic entry points, while `JsonRpcServer` / `JsonRpcClient` remain JSON-RPC backend aliases.
    *   Added `NekoRpcBackend<Serializer>` / `BinaryRpcBackend`, a compact binary RPC frame with a replaceable payload serializer.

*   **v0.2.4**
    *   split serialization and protocol management, with serialization as a core module and protocol management as an optional module.
    *   complete JSON-RPC 2.0 protocol support.
        *   support named parameter passing.
        *   support built-in methods: `rpc.get_method_list`, `rpc.get_bind_method_list`, `rpc.get_method_info`, `rpc.get_method_info_list`.
    *   add the is_skippable trait. mark a type can skippable like optional
    *   optimize jsonrpc template expansion, enable /bigobj option to compile

*   **v0.2.3**
    *   Unified most serialization calls to parenthesis expression `serializer(variable)`.
    *   Adjusted node expansion rules for `NameValuePair` and nested objects.
    *   Supported simdjson as JSON input serialization backend (`simdjson::dom`).
    *   Supported almost all common STL containers.
    *   Added general support for `std::optional`.
    *   Refactored communication interface to be a wrapper around transport protocols, separating underlying connection management.
    *   Added option to execute serialization/deserialization during communication in separate threads.
    *   Added build options to control feature enablement and logging output.
    *   Added UDP communication support.
    *   Added protocol table synchronization support (sender can share protocol table, receiver deserializes accordingly).
    *   Reserved internal protocol type IDs (1-64) for control messages.
    *   Improved binary protocol, fixed serialization issues for some types.
    *   Added more flags in protocol transmission (Flag), supporting protocol table exchange, handling unknown protocols as binary packets, and stream slicing transmission with cancellation.
    *   Supported XML deserialization.
    *   Added JSON-RPC 2.0 support.

*   **v0.2.0 - alpha**
    *   Modified serializer interface, unified to overloaded parenthesis expressions, replaced `Convert` class with `save`/`load` functions.
    *   Initial optimization of deserialization performance.
    *   Reduced space overhead of protocol base class.
    *   Supported TCP communication channel.

*   **v0.1.0**
    *   Changed protocol base class to use composition.
    *   Added field reflection support.
    *   Added support for `std::optional`, `std::variant`.

---

## 10. Contribution

Contributions via Pull Requests or Issues are welcome to improve this library!

---

## 11. License

![GitHub License](https://img.shields.io/github/license/liuli-neko/NekoProtoTools)

This project is released under the MIT License. See [LICENSE](LICENSE).
