# NekoProtoTools: A C++ Protocol Helper Library

[![中文](https://img.shields.io/badge/语言-中文-blue.svg)](./README.md)
[![English](https://img.shields.io/badge/language-English-blue.svg)](./README_en.md)

### CI Status

| Build Platform | Status | Code Coverage |
| :----------- | :----- | :------------ |
| Linux        | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-linux.yml) | [![codecov](https://codecov.io/gh/liuli-neko/NekoProtoTools/graph/badge.svg?token=F5OR647TV7)](https://codecov.io/gh/liuli-neko/NekoProtoTools) |
| Windows      | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-on-windows.yml) | |
| Clang-cl | [![Build Status](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml/badge.svg?branch=main)](https://github.com/liuli-neko/neko-proto-tools/actions/workflows/xmake-test-clang-cl.yml) | | 

---

## 1. Introduction

NekoProtoTools is a pure C++ protocol helper library designed to **simplify the definition, serialization/deserialization, and RPC communication of messages (protocols) in C++**.

Core features of this library:

*   **Simplified Message Definition**: Using templates and macros, you only need to define the fields of your message and declare the members to be processed. Any custom C++ type can then be used as a protocol message.
*   **Out-of-the-Box Serialization/Deserialization**: Built-in support for common serialization formats like JSON (RapidJSON/SIMDJson) and binary, with easy extension for custom serializers.
*   **Basic Reflection Capability**: Provides a simple reflection mechanism, allowing protocol handling at runtime using name information.
*   **Lightweight JSON-RPC 2.0 Implementation**: Based on templates and concise interfaces, allowing for convenient definition and implementation of RPC services.
*   **Type Safety**: Utilizes C++ template metaprogramming for compile-time type checking.

**Why This Library?**: Developers should focus their energy on business logic itself, rather than the tedious and error-prone construction and maintenance of protocols. This library aims to provide a stable, testable infrastructure to reduce the complexity of protocol handling.

---

## 2. Dependencies

*   **Serialization Backends**:
    *   JSON: [RapidJSON](https://rapidjson.org/) or [SIMDJson](https://simdjson.org/) (Optional, enable via build options)
    *   XML: [RapidXML](https://github.com/dwd/rapidxml) (Optional, enable via build options)
    *   *Note: To keep the library lightweight, these serialization libraries are **not directly bundled**. You need to manage these dependencies yourself through options.*
*   **Communication & RPC (Optional)**:
    *   [Ilias](https://github.com/BusyStudent/ilias) (Used for network communication and asynchronous tasks/coroutines)
*   **log (Optional)**:
    *   [format](https://en.cppreference.com/w/cpp/utility/format/format) (If std::format is available in the compilation environment, you can enable logging directly)
    *   [fmt](https://fmt.dev/) (Use fmt as the logging backend)
    *   [spdlog](https://github.com/gabime/spdlog) (Use spdlog as the logging backend)

---

## 3. Installation & Integration (Example)

This library recommends using [xmake](https://xmake.io/) for building and integration.

Add the dependency in your `xmake.lua`:

```lua
-- xmake.lua
add_requires("neko-proto-tools", {
    configs = {
         enable_simdjson = false,    -- Set to true to enable SIMDJson backend
         enable_rapidjson = true,    -- Set to true to enable RapidJSON backend
         enable_fmt = true,          -- Set to true if fmtlib support is needed (e.g., for logging)
         enable_communication = true,-- Set to true to enable communication features
         enable_jsonrpc = true       -- Set to true to enable JSON-RPC features
    },
    -- You might need to specify the git repo or use a repository like Btk-Project
    -- {git = "https://github.com/liuli-neko/neko-proto-tools.git", branch = "main"}
    -- Add the repository containing the package if it's not in the default xmake-repo
    -- add_repositories("btk-repo https://github.com/Btk-Project/xmake-repo.git")
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

Only include the basic header file and use the `NEKO_SERIALIZER` macro to mark members for serialization.

```cpp
#include <nekoproto/proto/serializer_base.hpp>
#include <nekoproto/proto/json_serializer.hpp> // Use JSON serializer
#include <nekoproto/proto/types/string.hpp>    // Support for std::string
#include <nekoproto/proto/types/vector.hpp>    // Support for std::vector
// #include <nekoproto/proto/types/types.hpp>  // Includes all supported type headers
#include <iostream>
#include <string>
#include <vector>

// Define your data structure
struct MyData {
    int         id;
    std::string name;
    double      score;

    // Declare members to be serialized
    NEKO_SERIALIZER(id, name, score);
};

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

If you need protocol management, reflection, and polymorphism support, include `proto_base.hpp` and use the `NEKO_DECLARE_PROTOCOL` macro.

```cpp
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/proto/serializer_base.hpp>
#include <nekoproto/proto/json_serializer.hpp> // Specify default serializer
#include <nekoproto/proto/types/string.hpp>
#include <iostream>
#include <string>
#include <vector>

// Define protocol message struct
struct UserProfile {
    int         userId;
    std::string username;

    // 1. Declare serialization members
    NEKO_SERIALIZER(userId, username);
    // 2. Declare as a protocol and specify the default serializer
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

    // --- Use the factory to create and manage protocols ---
    // Specify protocol version when creating the factory (major, minor, patch)
    ProtoFactory factory(1, 0, 0);

    // 1. Create UserProfile instance (via protocol base class interface)
    // emplaceProto returns an IProto interface pointer wrapping the specific protocol object
    // You can pass constructor arguments directly to emplaceProto
    auto user_iproto = UserProfile::emplaceProto(123, "Alice");

    // Serialize to binary data (using the protocol's specified default serializer: JsonSerializer)
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

Serializers are responsible for converting C++ objects into byte streams (serialization) and converting byte streams back into C++ objects (deserialization).

*   **Built-in Serializers**:
    *   `JsonSerializer`: Uses RapidJSON or SIMDJson for JSON serialization/deserialization.
    *   `BinarySerializer`: Provides a compact binary serialization format.
    *   `XmlSerializer`: Provides XML deserialization support (currently deserialization only).
*   **Choosing a Serializer**:
    *   For basic serialization, directly instantiate the required `OutputSerializer` / `InputSerializer`.
    *   For protocol messages (`NEKO_DECLARE_PROTOCOL`), specify the default serializer during declaration.
*   **Custom Serializers**: You can implement the `detail::OutputSerializer<YourSerializer>` and `detail::InputSerializer<YourSerializer>` interfaces to support custom formats. See [7. Custom Serializer](#7-custom-serializer).

### 5.2. Protocol Management (`ProtoFactory`, `IProto`)

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

### 5.3. Communication (`Communication`)

This library provides a coroutine-based communication abstraction layer built upon [Ilias](https://github.com/BusyStudent/ilias), designed for conveniently transmitting protocol messages over network connections.

*   **Required Header**: `#include <nekoproto/communication/communication_base.hpp>`
*   **Core Class**: `ProtoStreamClient<UnderlyingStream>` (e.g., `ProtoStreamClient<TcpClient>`)
*   **Functionality**: Wraps underlying network streams (like TCP, UDP), automatically handling the packing (adding type info and length), sending, receiving, and unpacking of protocol messages.

**Example (TCP Communication)**:

> Full example code: `tests/unit/communication/test_communication.cpp`

```cpp
#include <nekoproto/communication/communication_base.hpp>
#include <nekoproto/proto/proto_base.hpp>
#include <nekoproto/proto/json_serializer.hpp>
#include <nekoproto/proto/types/string.hpp>
#include <nekoproto/proto/types/vector.hpp> // Need to include headers for all used types

#include <ilias/net.hpp>         // Ilias network library
#include <ilias/platform.hpp>  // Ilias platform context
#include <ilias/task.hpp>        // Ilias coroutine/task library

#include <iostream>
#include <vector>
#include <string>
#include <chrono> // For timestamp

NEKO_USE_NAMESPACE // Use nekoproto namespace
using namespace ILIAS_NAMESPACE; // Use ilias namespace

// Define the message protocol to be transmitted
class ChatMessage {
public:
    uint64_t    timestamp;
    std::string sender;
    std::string content;

    NEKO_SERIALIZER(timestamp, sender, content);
    NEKO_DECLARE_PROTOCOL(ChatMessage, JsonSerializer); // Declare as a protocol
};

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

### 5.4. JSON-RPC 2.0

This library provides a lightweight JSON-RPC 2.0 implementation based on Ilias, allowing for easy definition and invocation of remote procedures.

*   **Required Header**: `#include <nekoproto/jsonrpc/jsonrpc.hpp>`
*   **Core Concepts**:
    *   **RPC Module Definition**: Use a `struct` to define a module containing all RPC methods.
    *   **`RpcMethod<Signature, "method_name">`**: Declare an RPC method within the module, specifying its function signature (`Signature`) and its method name in the JSON-RPC protocol (`"method_name"`). Supports all serializable types as parameters and return values.
    *   **`JsonRpcServer<ModuleType>`**: RPC server class used to host the RPC module and handle client requests.
    *   **`JsonRpcClient<ModuleType>`**: RPC client class used to connect to the server and invoke remote methods.
    *   **Async Support**: Method implementations can return `ilias::Task<ResultType>` or `ilias::IoTask<ResultType>` to support asynchronous operations.
*  **Server built-in methods**: 
    * rpc.get_method_list:      Get a list of all methods on the current server
    * rpc.get_bind_method_list: Get a list of all bound methods on the current server
    * rpc.get_method_info:      Get a description of a specified method
    * rpc.get_method_info_list: Get descriptions of all methods


**Example**:

```cpp
#include <nekoproto/jsonrpc/jsonrpc.hpp>
#include <nekoproto/proto/types/vector.hpp>   // Support for vector
#include <nekoproto/proto/types/string.hpp>  // Support for string
#include <ilias/platform.hpp>
#include <ilias/task.hpp>
#include <numeric> // for std::accumulate
#include <iostream>
#include <vector>
#include <string>

NEKO_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;

// 1. Define RPC service module and methods
struct CalculatorModule {
    // Method name "add", accepts two ints, returns int
    RpcMethod<int(int, int), "add"> add;
    // Method name "subtract", accepts two ints, returns int
    RpcMethod<int(int, int), "subtract"> subtract;
    // Method name "sum", accepts vector<int>, returns int (async implementation)
    RpcMethod<int(std::vector<int>), "sum"> sum;
    // Method name "greet", accepts string, returns string
    RpcMethod<std::string(std::string), "greet"> greet;
};

// 2. Implement server logic
ilias::Task<> run_server(PlatformContext& context) {
    JsonRpcServer<CalculatorModule> rpc_server; // Create server instance

    // --- Bind method implementations ---
    // Can use lambdas, function pointers, std::function, etc.
    rpc_server->add = [](int a, int b) -> int {
        std::cout << "Server: add(" << a << ", " << b << ") called." << std::endl;
        return a + b;
    };

    rpc_server->subtract = [](int a, int b) {
         std::cout << "Server: subtract(" << a << ", " << b << ") called." << std::endl;
        return a - b;
    };

    // Coroutine method binding, returns ilias::IoTask<> (runs in IO context)
    rpc_server->sum = [](std::vector<int> vec) -> ilias::IoTask<int> {
        std::cout << "Server: sum(...) called asynchronously." << std::endl;
        // Can execute other coroutine functions using co_await here
        int result = std::accumulate(vec.begin(), vec.end(), 0);
        co_return result; // Use co_return to return the result
    };

     rpc_server->greet = [](std::string name) -> std::string {
         std::cout << "Server: greet(\"" << name << "\") called." << std::endl;
         return "Hello, " + name + "!";
     };

    // Start the server, listening on the specified address and port
    std::string listen_address = "tcp://127.0.0.1:12335";
    auto start_result = co_await rpc_server.start(listen_address);
    if (!start_result) {
        std::cerr << "Server failed to start: " << start_result.error().message() << std::endl;
        co_return;
    }
    std::cout << "RPC Server listening on " << listen_address << std::endl;

    // Wait for server stop signal (e.g., Ctrl+C or explicit stop)
    co_await rpc_server.wait();
    std::cout << "RPC Server stopped." << std::endl;
}

// 3. Implement client logic
ilias::Task<> run_client(PlatformContext& context) {
    JsonRpcClient<CalculatorModule> rpc_client; // Create client instance

    // Connect to the server
    std::string server_address = "tcp://127.0.0.1:12335";
    auto connect_result = co_await rpc_client.connect(server_address);
    if (!connect_result) {
        std::cerr << "Client failed to connect: " << connect_result.error().message() << std::endl;
        co_return;
    }
    std::cout << "Client connected to " << server_address << std::endl;

    // --- Call remote methods ---
    // Call add method
    // The result is wrapped in ilias::Result<>
    auto add_result = co_await rpc_client->add(10, 5);
    if (add_result) {
        std::cout << "Client: add(10, 5) = " << add_result.value() << std::endl; // Output: 15
    } else {
        // Handle error, e.g., connection issue or server-side exception
        std::cerr << "Client: add call failed: " << add_result.error().message() << std::endl;
    }

    // Call sum method (async on server)
    std::vector<int> nums = {1, 2, 3, 4, 5};
    auto sum_result = co_await rpc_client->sum(nums);
    if (sum_result) {
        std::cout << "Client: sum({1,2,3,4,5}) = " << sum_result.value() << std::endl; // Output: 15
    } else {
         std::cerr << "Client: sum call failed: " << sum_result.error().message() << std::endl;
    }

    // Call greet method
    auto greet_result = co_await rpc_client->greet("Neko");
    if (greet_result) {
        std::cout << "Client: greet(\"Neko\") = \"" << greet_result.value() << "\"" << std::endl; // Output: "Hello, Neko!"
    } else {
         std::cerr << "Client: greet call failed: " << greet_result.error().message() << std::endl;
    }

    // Calling a non-existent method:
    // Direct call like `rpc_client->multiply(2, 3)` would cause a compile error
    // as `multiply` is not defined in `CalculatorModule`.
    // To simulate calling a method the server doesn't have, you'd need manual
    // JSON-RPC request construction, which isn't shown here. The server would
    // typically return a standard JSON-RPC error ("Method not found").

    rpc_client.close(); // Disconnect (closes underlying connection)
    std::cout << "Client disconnected." << std::endl;
}

int main() {
    PlatformContext context;

    // Start server and client tasks
    ilias_go run_server(context);
    // Add delay before starting client
    ilias_go ilias::sleep_for(std::chrono::milliseconds(100)).then([&]() {
        return run_client(context);
    });


    context.run(); // Run event loop

    // Server might need explicit stopping mechanism in a real application
    return 0;
}

```

---

## 6. Supported Types

This library provides serialization support for numerous C++ standard library types through dedicated header files. You typically need to include the header for the specific type(s) you use (e.g., `nekoproto/proto/types/vector.hpp` for `std::vector`) or include `nekoproto/proto/types/types.hpp` to get all of them.

### 6.1. JSON Serializer (`JsonSerializer`)

| C++ Type                             | Supported | JSON Type          | Include Header                         | Notes                                      |
| :----------------------------------- | :-------: | :----------------- | :------------------------------------- | :----------------------------------------- |
| `bool`                               | ✅        | boolean            | `json_serializer.hpp`                  |                                            |
| `int8_t`, `int16_t`, `int32_t`, `int64_t` | ✅        | number (integer)   | `json_serializer.hpp`                  |                                            |
| `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`| ✅        | number (integer)   | `json_serializer.hpp`                  |                                            |
| `float`, `double`                    | ✅        | number (float)     | `json_serializer.hpp`                  |                                            |
| `std::string`                        | ✅        | string             | `types/string.hpp`                     |                                            |
| `std::u8string` (C++20)              | ✅        | string             | `types/u8string.hpp`                   | Requires C++20                             |
| `std::vector<T>`                     | ✅        | array              | `types/vector.hpp`                     | `T` must be a supported type             |
| `class : IProto`                     | ✅        | object             | `types/binary_data.hpp`                | As nested protocol object                  |
| `std::array<T, N>`                   | ✅        | array              | `types/array.hpp`                      | `T` must be a supported type             |
| `std::set<T>`                        | ✅        | array              | `types/set.hpp`                        | `T` must be a supported type             |
| `std::list<T>`                       | ✅        | array              | `types/list.hpp`                       | `T` must be a supported type             |
| `std::map<std::string, T>`           | ✅        | object             | `types/map.hpp`                        | `T` must be a supported type             |
| `std::map<K, V>`                     | ✅        | array of `[K, V]`  | `types/map.hpp`                        | `K` not `std::string`, `K`, `V` must be supported |
| `std::tuple<T...>`                   | ✅        | array              | `types/tuple.hpp`                      | All `T` must be supported types          |
| Custom `struct`/`class`              | ✅        | object             | *(Auto-supported via `NEKO_SERIALIZER`)* | Members must be supported types            |
| `enum class`/`enum`                  | ✅        | string or number   | `types/enum.hpp`                       | Serialized as string by default, configurable as integer |
| `std::optional<T>`                   | ✅        | T or null          | `json_serializer.hpp`                  | `T` must be a supported type             |
| `std::variant<T...>`                 | ✅        | object (`{idx:I, val:V}`)| `types/variant.hpp`                | All `T` must be supported types          |
| `std::pair<T1, T2>`                  | ✅        | object (`{key:V1, val:V2}`)| `types/pair.hpp`                   | `T1`, `T2` must be supported types       |
| `std::bitset<N>`                     | ✅        | string             | `types/bitset.hpp`                     | Serialized as "01..." string             |
| `std::shared_ptr<T>`                 | ✅        | T or null          | `types/shared_ptr.hpp`                 | `T` must be a supported type             |
| `std::unique_ptr<T>`                 | ✅        | T or null          | `types/unique_ptr.hpp`                 | `T` must be a supported type             |
| `std::atomic<T>`                     | ✅        | T                  | `types/atomic.hpp`                     | Serializes its contained value `T`         |
| `std::unordered_set<T>`              | ✅        | array              | `types/unordered_set.hpp`              | `T` must be a supported type             |
| `std::unordered_map<std::string, T>` | ✅        | object             | `types/unordered_map.hpp`              | `T` must be a supported type             |
| `std::unordered_map<K, V>`           | ✅        | array of `[K, V]`  | `types/unordered_map.hpp`              | `K` not `std::string`, `K`, `V` must be supported |
| `std::multiset<T>`                   | ✅        | array              | `types/multiset.hpp`                   | `T` must be a supported type             |
| `std::multimap<K, V>`                | ✅        | array of `[K, V]`  | `types/multimap.hpp`                   | `K`, `V` must be supported                 |
| `std::unordered_multiset<T>`         | ✅        | array              | `types/unordered_multiset.hpp`         | `T` must be a supported type             |
| `std::unordered_multimap<K, V>`      | ✅        | array of `[K, V]`  | `types/unordered_multimap.hpp`         | `K`, `V` must be supported                 |
| `std::deque<T>`                      | ✅        | array              | `types/deque.hpp`                      | `T` must be a supported type             |
| `std::any`                           | ❌        | -                  | -                                      | Not supported (Type Erasure)               |

*   `T`, `K`, `V` represent any already supported type.
*   For `map` and `unordered_map`, if the key is not `std::string`, it's serialized as an array of key-value pairs: `[[key1, value1], [key2, value2], ...]`.

### 6.2. Binary Serializer (`BinarySerializer`)

| C++ Type                         | Supported | Size (Bytes)           | Include Header          | Notes                                     |
| :------------------------------- | :-------: | :--------------------- | :---------------------- | :---------------------------------------- |
| `bool`                           | ✅        | 1                      | `binary_serializer.hpp` |                                           |
| `int8_t`, `uint8_t`              | ✅        | 1                      | `binary_serializer.hpp` |                                           |
| `int16_t`, `uint16_t`            | ✅        | 2                      | `binary_serializer.hpp` | Network Byte Order (Big Endian)           |
| `int32_t`, `uint32_t`            | ✅        | 4                      | `binary_serializer.hpp` | Network Byte Order (Big Endian)           |
| `int64_t`, `uint64_t`            | ✅        | 8                      | `binary_serializer.hpp` | Network Byte Order (Big Endian)           |
| `float`                          | ✅        | 4                      | `binary_serializer.hpp` | IEEE 754, Network Byte Order (Big Endian) |
| `double`                         | ✅        | 8                      | `binary_serializer.hpp` | IEEE 754, Network Byte Order (Big Endian) |
| `std::string`                    | ✅        | 4 (length) + N (content) | `types/string.hpp`      | Length uses `uint32_t`, network byte order|
| `enum class`/`enum`              | ✅        | Depends on underlying type | `types/enum.hpp`        | Serialized as underlying integer type     |
| Containers (vector, list, map etc.)| ✅        | 4 (size) + elements size | `types/*.hpp`         | Size uses `uint32_t`, network byte order  |
| `std::optional<T>`               | ✅        | 1 (flag) + [sizeof(T)] | `binary_serializer.hpp` | Stores T's data additionally when present |
| `struct`/`class`                 | ✅        | Sum of member sizes    | *(Auto-supported)*      | Serialized in `NEKO_SERIALIZER` order     |
| `NamePairValue<std::string, T>` | ✅        | 4+len(name) + len(T)   | `binary_serializer.hpp` | Special structure for certain scenarios   |

**Note**:

*   Container type support is similar to the JSON serializer; corresponding `types/*.hpp` headers are needed.
*   The binary serializer uses **Network Byte Order (Big Endian)** by default for multi-byte types.

### 6.3. XML Serializer (`XmlSerializer`)

*   Currently **only supports deserialization**.
*   Type support is basically the same as the BinarySerializer. Values are typically parsed from XML element/attribute text content.

---

## 7. Custom Serializer

If you need to support a serialization format not built into the library, you can implement a custom serializer. You need to inherit from `detail::OutputSerializer<CustomOutputSerializer>` and `detail::InputSerializer<CustomInputSerializer>` and implement their interfaces.

**Output Serializer Interface Skeleton**:

```cpp
#include <nekoproto/proto/serializer_base.hpp>
#include <vector>
#include <string>
#include <cstddef> // for std::size_t

using namespace NekoProto; // or NekoProto::detail

class CustomOutputSerializer : public detail::OutputSerializer<CustomOutputSerializer> {
public:
    // Constructor, typically receives the output buffer
    CustomOutputSerializer(std::vector<char>& out_buffer);

    // --- Core save functions ---
    // Save various basic types
    bool saveValue(int8_t value); // Note: pass by value for fundamentals
    bool saveValue(uint8_t value);
    // ... other integer types (int16, uint16, int32, uint32, int64, uint64)
    bool saveValue(float value);
    bool saveValue(double value);
    bool saveValue(bool value);
    bool saveValue(const std::string& value); // Pass by const reference for objects
    // bool saveValue(const char* value); // Optional
    // bool saveValue(std::string_view value); // Optional (C++17+)

    // Save named value (for object/struct members)
    // Needs to handle the case where T is std::optional and has no value (usually skip output)
    template <typename T>
    bool saveValue(const NameValuePair<T>& value);

    // --- Container/Structure Control ---
    // Start array/list, size is the number of elements
    bool startArray(std::size_t size);
    // End array/list
    bool endArray();
    // Start object/struct, size is the number of members (might be -1 if unknown beforehand)
    bool startObject(std::size_t size);
    // End object/struct
    bool endObject();

    // --- Special Tag Handling ---
    // Save container size tag (often used during array serialization, consistent with size in startArray)
    template <typename T>
    bool saveValue(SizeTag<T> const& size);

    // End the entire serialization process, ensure all buffers are written
    // Destructor should also ensure end() is called or writing is completed
    bool end();

private:
    // Add necessary member variables, e.g., output buffer reference, state info
    // std::vector<char>& m_buffer;

    // Forbid copy and move construction/assignment
    CustomOutputSerializer(const CustomOutputSerializer&) = delete;
    CustomOutputSerializer& operator=(const CustomOutputSerializer&) = delete;
    CustomOutputSerializer(CustomOutputSerializer&&) = delete;
    CustomOutputSerializer& operator=(CustomOutputSerializer&&) = delete;
};
```

**Input Serializer Interface Skeleton**:

```cpp
#include <nekoproto/proto/serializer_base.hpp>
#include <vector>
#include <string>
#include <cstddef> // for std::size_t
#include <string_view> // Example for input source

using namespace NekoProto; // or NekoProto::detail

class CustomInputSerializer : public detail::InputSerializer<CustomInputSerializer> {
public:
    // Constructor, typically receives the input buffer (pointer and size, or view)
    CustomInputSerializer(const char* input_buffer, std::size_t size);
    // Or use string_view or other input source
    // CustomInputSerializer(std::string_view input_data);

    // --- Status Check ---
    // Return if the serializer is in a valid state (e.g., hasn't reached end, no parse errors)
    explicit operator bool() const;

    // --- Core load functions ---
    // Load various basic types into the reference parameter
    bool loadValue(int8_t& value);
    bool loadValue(uint8_t& value);
    // ... other integer types (int16, uint16, int32, uint32, int64, uint64)
    bool loadValue(float& value);
    bool loadValue(double& value);
    bool loadValue(bool& value);
    bool loadValue(std::string& value);

    // Load named value (for object/struct members)
    // Needs to be able to find the value by name and load it into value.value
    template <typename T>
    bool loadValue(const NameValuePair<T>& value); // Load into value.value

    // --- Container/Structure Control ---
    // Enter a node/element (e.g., move into an array)
    // Might not be needed for all formats
    bool startNode();
    // Finish processing the current node/element (e.g., array fully read, return to parent)
    // Might not be needed for all formats
    bool finishNode();

    // --- Special Tag Handling (Optional) ---
    // Load container size tag (usually called before processing an array)
    template <typename T>
    bool loadValue(SizeTag<T>& value); // value.size will be populated

private:
    // Add necessary member variables, e.g., input data view, current position, parser state
    // std::string_view m_data;
    // std::size_t m_pos = 0;
    // Add state variables for the specific format, e.g., parsing stack, current JSON node, etc.

    // Forbid copy and move construction/assignment
    CustomInputSerializer(const CustomInputSerializer&) = delete;
    CustomInputSerializer& operator=(const CustomInputSerializer&) = delete;
    CustomInputSerializer(CustomInputSerializer&&) = delete;
    CustomInputSerializer& operator=(CustomInputSerializer&&) = delete;
};
```

---

## 8. TODO

**Serializer**

*   [x] Support accessing protocol fields by string name (basic reflection)
*   [x] Use SIMDJson as JSON input serializer backend (`simdjson::dom`)
*   [ ] Support `simdjson::ondemand` interface (Explore performance and use case differences with `dom`)
*   [x] Implement JSON output serializer based on SIMDJson (currently manual implementation, performance needs optimization)
*   [x] Support more C++ STL containers

**Communication**

*   [x] Support UDP communication channel (`ProtoDatagramClient`)
*   [ ] Support more underlying transport protocols (e.g., WebSocket, QUIC - possibly via Ilias or other libraries)
*   [ ] Enhance communication layer atomicity: Ensure complete processing of data frames, maintaining consistent data stream state even during cancellation operations. May require adjusting to a small-frame sending mechanism.

**JSON-RPC**

*   [x] Support JSON-RPC 2.0 protocol specification.
*   [ ] Compatibility with JSON-RPC 1.0 protocol (Optional).
*   [ ] Support for JSON-RPC extensions (e.g., custom error codes, metadata).

---

## 9. Development History (Selected Milestones)

*   **(Current Main)**
    *   Unified most serialization calls to parenthesis expression `serializer(variable)`.
    *   Special structures like `NameValuePair`, `SizeTag` no longer trigger node expansion; other objects without the `minimal_serializable` attribute trigger node expansion (like JSON nesting).
    *   Supported SIMDJson as JSON input serialization backend (`simdjson::dom`).
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
