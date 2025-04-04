/**
 * @file jsonrpc.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-29
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <functional>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility> // std::index_sequence, std::make_index_sequence

#include <nekoproto/proto/json_serializer.hpp>
#include <nekoproto/proto/serializer_base.hpp>
#include <nekoproto/proto/types/types.hpp>

#include <ilias/error.hpp>
#include <ilias/net/sockopt.hpp>
#include <ilias/net/system.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/net/udp.hpp>
#include <ilias/sync/event.hpp>
#include <ilias/sync/mutex.hpp>
#include <ilias/sync/scope.hpp>
#include <ilias/task.hpp>

NEKO_BEGIN_NAMESPACE
enum class JsonRpcError {
    Ok = 0,
    // Server error Reserved for implementation-defined server-errors.
    ServerErrorStart   = -32000,
    MethodNotBind      = -32000,
    ClientNotInit      = -32001,
    ResponseIdNotMatch = -32002,
    MessageToolLarge   = -32003, // Request too large The JSON sent is too big.
    ServerErrorEnd     = -32099, // Server error end

    ParseError = -32700, // Parse error Invalid JSON was received by the server. An error
    // occurred on the server while parsing the JSON text.
    InvalidRequest = -32600, // Invalid Request The JSON sent is not a valid Request object.
    MethodNotFound = -32601, // Method not found The method does not exist / is not available.
    InvalidParams  = -32602, // Invalid params Invalid method parameter(s).
    InternalError  = -32603, // Internal error Internal JSON-RPC error.
};

class JsonRpcErrorCategory : public ILIAS_NAMESPACE::ErrorCategory {
public:
    static JsonRpcErrorCategory& instance();
    auto message(int64_t value) const -> std::string override;
    auto name() const -> std::string_view override { return "jsonrpc"; }
};

inline JsonRpcErrorCategory& JsonRpcErrorCategory::instance() {
    static JsonRpcErrorCategory kInstance;
    return kInstance;
}

inline auto JsonRpcErrorCategory::message(int64_t value) const -> std::string {
    switch ((JsonRpcError)value) {
    case JsonRpcError::Ok:
        return "Ok";
    case JsonRpcError::MethodNotBind:
        return "Method not Bind";
    case JsonRpcError::ParseError:
        return "Parse Eror";
    case JsonRpcError::InvalidRequest:
        return "InvalidRequest";
    case JsonRpcError::MethodNotFound:
        return "Method ot Found";
    case JsonRpcError::InvalidParams:
        return "InvalidParams";
    case JsonRpcError::InternalError:
        return "Internal Error";
    case JsonRpcError::ClientNotInit:
        return "Client Not Init";
    case JsonRpcError::ResponseIdNotMatch:
        return "Response Id Not Match";
    default:
        return "Unknow Error";
    }
}

ILIAS_DECLARE_ERROR(JsonRpcError, JsonRpcErrorCategory);

class DatagramBase {
public:
    enum class Type { Server, Client };
    /**
     * @brief check if the url is valid
     * if return true, whill use this DatagramClient to connect
     * @param url url like "tcp://127.0.0.1:8080", Can be customized, but the protocol must be put first to prevent
     * overlap
     * @return true
     * @return false
     */
    virtual auto checkProtocol(Type type, std::string_view url) -> bool = 0;
    /**
     * @brief start the client or server
     *
     * @return ILIAS_NAMESPACE::IoTask<void>
     */
    virtual auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> = 0;
    /**
     * @brief close this connect
     *
     */
    virtual auto close() -> void = 0;
};

class DatagramClientBase {
public:
    virtual ~DatagramClientBase() = default;
    /**
     * @brief recv data from remote
     * must a valid jsonrpc request or response
     * @return ILIAS_NAMESPACE::IoTask<std::span<std::byte>>
     */
    virtual auto recv() -> ILIAS_NAMESPACE::IoTask<std::span<std::byte>> = 0;
    /**
     * @brief send jsonrpc request or response data to remote
     * must send all or this connect will be closed
     * @param buffer
     * @return ILIAS_NAMESPACE::IoTask<void>
     */
    virtual auto send(std::span<const std::byte> buffer) -> ILIAS_NAMESPACE::IoTask<void> = 0;
};

class DatagramServerBase {
public:
    virtual ~DatagramServerBase() = default;
    /**
     * @brief listen loop
     * you must make sure it return when the server is closed.
     * @return ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void(DatagramClientBase*)>>
     */
    virtual auto accept()
        -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> = 0;
};

template <typename T, std::size_t N = 1500>
class DatagramClient;
/**
 * @brief 数据包接收，负责拆包，默认为最大长度1500的UDP包
 *
 * @tparam T
 * @tparam N
 */
template <>
class DatagramClient<ILIAS_NAMESPACE::UdpClient, 1500>
    : public DatagramBase, public DatagramClientBase, public DatagramServerBase {
public:
    DatagramClient() { memset(buffer.data(), 0, 1500); }

    auto recv() -> ILIAS_NAMESPACE::IoTask<std::span<std::byte>> override {
        if (!client) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        auto ret =
            co_await (client.recvfrom({buffer.data(), buffer.size()}, endpoint) | ILIAS_NAMESPACE::ignoreCancellation);
        if (ret) {
            co_return std::span<std::byte>{buffer.data(), ret.value()};
        } else {
            co_return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ret.error());
        }
    }

    auto send(std::span<const std::byte> data) -> ILIAS_NAMESPACE::IoTask<void> override {
        if (!client) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (data.size() >= 1500) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::MessageToolLarge);
        }
        auto ret  = co_await (client.sendto(data, endpoint) | ILIAS_NAMESPACE::ignoreCancellation);
        auto send = ret.value_or(0);
        while (ret && send < data.size()) {
            ret = co_await (client.sendto({data.data() + send, data.size() - send}, endpoint) |
                            ILIAS_NAMESPACE::ignoreCancellation);
            if (ret && ret.value() > 0) {
                send += ret.value();
            } else {
                break;
            }
        }
        if (!ret) {
            co_return ILIAS_NAMESPACE::Unexpected<ILIAS_NAMESPACE::Error>(ret.error());
        }
        co_return {};
    }

    auto close() -> void override {
        if (client) {
            client.close();
        }
    }

    // like udp://127.0.0.1:12345-127.0.0.1:12346
    auto checkProtocol([[maybe_unused]] Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 6) != "udp://");
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        auto bindRemoteIp = url.substr(6);
        auto pos          = bindRemoteIp.find('-');
        if (pos == std::string_view::npos) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
        auto bindIpendpoint   = ILIAS_NAMESPACE::IPEndpoint::fromString(bindRemoteIp.substr(0, pos));
        auto remoteIpendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(bindRemoteIp.substr(pos + 1));
        if (!bindIpendpoint || !remoteIpendpoint) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
        if (auto ret = co_await ILIAS_NAMESPACE::UdpClient::make(bindIpendpoint->family()); ret) {
            client = std::move(ret.value());
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        client.setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
        client.bind(bindIpendpoint.value());
        endpoint = remoteIpendpoint.value();
        co_return {};
    }

    auto accept()
        -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> override {
        co_return std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>(this, [](DatagramClientBase*) {});
    }

    ILIAS_NAMESPACE::UdpClient client;
    ILIAS_NAMESPACE::IPEndpoint endpoint;
    std::array<std::byte, 1500> buffer;
};

/**
 * @brief 数据包接收，负责拆包
 *
 * @tparam T
 * @tparam N
 */
template <>
class DatagramClient<ILIAS_NAMESPACE::TcpClient, 1500> : public DatagramBase, public DatagramClientBase {
public:
    DatagramClient() = default;
    DatagramClient(ILIAS_NAMESPACE::TcpClient&& client) : client(std::move(client)) {}

    auto recv() -> ILIAS_NAMESPACE::IoTask<std::span<std::byte>> override {
        if (!client) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        uint32_t size = 0;
        if (auto ret = co_await (client.readAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) |
                                 ILIAS_NAMESPACE::ignoreCancellation);
            !ret) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        } else if (ret.value() == sizeof(size)) {
            size = ILIAS_NAMESPACE::networkToHost(size);
        }
        if (buffer.size() < size) {
            buffer.resize(size);
        }
        auto ret = co_await (client.readAll({buffer.data(), size}) | ILIAS_NAMESPACE::ignoreCancellation);
        if (ret && ret.value() == size) {
            co_return std::span<std::byte>{reinterpret_cast<std::byte*>(buffer.data()), size};
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error::Unknown));
        }
    }

    auto send(std::span<const std::byte> data) -> ILIAS_NAMESPACE::IoTask<void> override {
        if (!client) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        auto size = ILIAS_NAMESPACE::hostToNetwork((uint32_t)data.size());
        if (auto ret = co_await (client.writeAll({reinterpret_cast<std::byte*>(&size), sizeof(size)}) |
                                 ILIAS_NAMESPACE::ignoreCancellation);
            !ret) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        } else if (ret.value() != sizeof(size)) {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::Unknown);
        }
        auto ret = co_await (client.writeAll(data) | ILIAS_NAMESPACE::ignoreCancellation);
        if (!ret || ret.value() != data.size()) {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error_or(ILIAS_NAMESPACE::Error::Unknown));
        }
        co_return {};
    }
    auto close() -> void override {
        if (client) {
            client.close();
        }
    }

    auto checkProtocol([[maybe_unused]] Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 6) != "tcp://") && type == Type::Client;
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        auto ipendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(url.substr(6));
        if (!ipendpoint) {
            NEKO_LOG_ERROR("jsonrpc", "invalid endpoint: {}", url);
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
        if (auto ret = co_await ILIAS_NAMESPACE::TcpClient::make(ipendpoint->family()); ret) {
            if (auto ret1 = co_await ret.value().connect(ipendpoint.value()); ret1) {
                client = std::move(ret.value());
                co_return {};
            } else {
                NEKO_LOG_ERROR("jsonrpc", "tcpclient start failed, {}", ret1.error().message());
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
        } else {
            NEKO_LOG_ERROR("jsonrpc", "tcpclient start failed, {}", ret.error().message());
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        co_return {};
    }

    ILIAS_NAMESPACE::TcpClient client;
    std::vector<std::byte> buffer;
};

template <>
class DatagramClient<ILIAS_NAMESPACE::TcpListener, 1500> : public DatagramBase, public DatagramServerBase {
public:
    DatagramClient() {}

    auto close() -> void override {
        if (listener) {
            listener.close();
        }
    }

    auto checkProtocol(Type type, std::string_view url) -> bool override {
        return !(url.substr(0, 6) != "tcp://") && type == Type::Server;
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<void> override {
        auto ipendpoint = ILIAS_NAMESPACE::IPEndpoint::fromString(url.substr(6));
        if (!ipendpoint) {
            NEKO_LOG_ERROR("jsonrpc", "invalid endpoint: {}", url);
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
        if (auto ret = co_await ILIAS_NAMESPACE::TcpListener::make(ipendpoint->family()); ret) {
            ret.value().setOption(ILIAS_NAMESPACE::sockopt::ReuseAddress(1));
            if (auto ret1 = ret.value().bind(ipendpoint.value()); ret1) {
                listener = std::move(ret.value());
                co_return {};
            } else {
                NEKO_LOG_ERROR("jsonrpc", "tcpclient start failed, {}", ret1.error().message());
                co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
            }
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        co_return {};
    }

    auto accept()
        -> ILIAS_NAMESPACE::IoTask<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> override {
        if (!listener) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (auto ret = co_await listener.accept(); ret) {
            co_return std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>(
                new DatagramClient<ILIAS_NAMESPACE::TcpClient>(std::move(ret.value().first)),
                +[](DatagramClientBase* ptr) { delete ptr; });
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }

    ILIAS_NAMESPACE::TcpListener listener;
    std::vector<std::byte> buffer;
};

namespace traits {
template <typename T>
concept Serializable = requires(NekoProto::JsonSerializer::OutputSerializer serializer, T value) {
    { serializer(value) };
};
template <typename T, class enable = void>
struct IsSerializable : std::false_type {};
template <Serializable T>
struct IsSerializable<T, void> : std::true_type {};
template <>
struct IsSerializable<void> : std::true_type {};

template <typename T, class enable = void>
class RpcMethodTraits;

template <typename RetT, typename... Args>
    requires IsSerializable<std::tuple<std::remove_cvref_t<Args>...>>::value &&
             IsSerializable<std::remove_cvref_t<RetT>>::value
class RpcMethodTraits<RetT(Args...), void> {
public:
    using ParamsTupleType = std::tuple<std::remove_cvref_t<Args>...>;
    using ReturnType =
        std::optional<std::conditional_t<std::is_void_v<RetT>, std::nullptr_t, std::remove_cvref_t<RetT>>>;
    using FunctionType       = std::function<RetT(Args...)>;
    using RawReturnType      = RetT;
    using RawParamsType      = std::tuple<Args...>;
    using CoroutinesFuncType = std::function<ILIAS_NAMESPACE::IoTask<RawReturnType>(Args...)>;

    auto operator()(Args... args) const -> ILIAS_NAMESPACE::IoTask<RawReturnType> {
        if (mCoFunction) {
            co_return co_await mCoFunction(std::forward<Args>(args)...);
        }
        co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::MethodNotBind);
    }

    auto notification(Args... args) const -> ILIAS_NAMESPACE::IoTask<RawReturnType> {
        mIsNotification = true;
        if (mCoFunction) {
            co_await mCoFunction(std::forward<Args>(args)...);
        }
        mIsNotification = false;
        co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::MethodNotBind);
    }

    auto isNotification() const -> bool { return mIsNotification; }

protected:
    CoroutinesFuncType mCoFunction;
    mutable bool mIsNotification = false;
};

template <typename RetT>
    requires IsSerializable<std::remove_cvref_t<RetT>>::value
class RpcMethodTraits<RetT(void), void> {
public:
    using ParamsTupleType = std::optional<std::array<int, 0>>;
    using ReturnType =
        std::optional<std::conditional_t<std::is_void_v<RetT>, std::nullptr_t, std::remove_cvref_t<RetT>>>;
    using FunctionType       = std::function<RetT()>;
    using RawReturnType      = RetT;
    using RawParamsType      = std::tuple<>;
    using CoroutinesFuncType = std::function<ILIAS_NAMESPACE::IoTask<RawReturnType>()>;

    auto operator()() const noexcept -> ILIAS_NAMESPACE::IoTask<RawReturnType> {
        if (mCoFunction) {
            co_return co_await mCoFunction();
        }
        co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::MethodNotBind);
    }

    auto notification() const -> ILIAS_NAMESPACE::IoTask<> {
        std::unique_ptr<bool, std::function<void(bool*)>> raii((bool*)(mIsNotification = true),
                                                               [this](bool*) { mIsNotification = false; });
        if (mCoFunction) {
            if (auto ret = co_await mCoFunction(); !ret) {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
        }
        co_return {};
    }

    auto isNotification() const -> bool { return mIsNotification; }

protected:
    CoroutinesFuncType mCoFunction;
    mutable bool mIsNotification = false;
};

template <typename T>
concept RpcMethod = requires() { std::is_constructible_v<typename RpcMethodTraits<T>::FunctionType, T>; };

// Helper to apply a function `func` to each element of a tuple `t`
// `func` should be a generic callable (like a template lambda)
template <typename Tuple, typename Func>
constexpr void for_each_tuple_element(Tuple&& tuple, Func&& func) {
    // Get tuple size at compile time
    constexpr std::size_t Ns = std::tuple_size_v<std::remove_cvref_t<Tuple>>;

    // Generate index sequence 0, 1, ..., Ns-1
    // Use an immediately-invoked lambda to capture the index pack Is...
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        // Use a fold expression over the comma operator
        // For each index I in Is..., call func(std::get<I>(tuple))
        ((void)std::forward<Func>(func)(std::get<Is>(std::forward<Tuple>(tuple))), ...);
        // The (void) cast suppresses potential warnings about unused result of comma operator
        // std::forward preserves value category of tuple and func
    }(std::make_index_sequence<Ns>{});
}

// --- MethodNameString Implementation (C++20 NTTP) ---
template <std::size_t N>
struct MethodNameString {
    std::array<char, N + 1> data{};

    // consteval 构造函数，确保编译时创建
    consteval MethodNameString(const char* str) noexcept {
        std::size_t actualLen = 0;
        // 复制直到 null 或达到 N
        while (str[actualLen] != '\0' && actualLen < N) {
            data[actualLen] = str[actualLen];
            actualLen++;
        }
        // 如果有空间，添加 null 终止符 (string_view 不需要，但 c_str 可能需要)
        if (actualLen <= N) {
            data[actualLen] = '\0';
        }
        // C++20 要求 NTTP 类型的所有基类和非静态数据成员都是 public 的
        // 并且类型是结构性相等的 (structural equality) - 默认即可
    }

    // 比较运算符对 NTTP 至关重要
    constexpr auto operator<=>(const MethodNameString&) const = default;
    constexpr bool operator==(const MethodNameString&) const  = default;

    // 访问器
    [[nodiscard]]
    constexpr std::size_t size() const noexcept {
        return N;
    }
    [[nodiscard]]
    constexpr std::string_view view() const noexcept {
        return std::string_view(data.data(), N);
    }
    [[nodiscard]]
    constexpr const char* c_str() const noexcept { // NOLINT
        return data.data();
    }
};

// CTAD 推导指引，方便从字面量创建 MethodNameString (去掉末尾 '\0')
template <std::size_t N>
MethodNameString(const char (&)[N]) -> MethodNameString<N - 1>;

// The Client is defined as the origin of Request objects and the handler of Response objects.
/**
 * @brief JsonRpc Request or Notification
 * @par Request
 * @param jsonrpc A String specifying the version of the JSON-RPC protocol. MUST be exactly
 * "2.0".
 * @param method  A String containing the name of the method to be invoked. Method names that
 * begin with the word rpc followed by a period character (U+002E or ASCII 46) are reserved for
 * rpc-internal methods and extensions and MUST NOT be used for anything else.
 * @param params  A Structured value that holds the parameter values to be used during the
 * invocation of the method. This member MAY be omitted.
 * @param id An identifier established by the Client that MUST contain a String, Number, or NULL
 * value if included. If it is not included it is assumed to be a notification. The value SHOULD
 * normally not be Null [1] and Numbers SHOULD NOT contain fractional parts [2] The Server MUST
 * reply with the same value in the Response object if included. This member is used to
 * correlate the context between the two objects. [1] The use of Null as a value for the id
 * member in a Request object is discouraged, because this specification uses a value of Null
 * for Responses with an unknown id. Also, because JSON-RPC 1.0 uses an id value of Null for
 * Notifications this could cause confusion in handling. [2] Fractional parts may be
 * problematic, since many decimal fractions cannot be represented exactly as binary fractions.
 * @par Notification
 * A Notification is a Request object without an "id" member. A Request object that is a
 * Notification signifies the Client's lack of interest in the corresponding Response object,
 * and as such no Response object needs to be returned to the client. The Server MUST NOT reply
 * to a Notification, including those that are within a batch request.
 * Notifications are not confirmable by definition, since they do not have a Response object to
 * be returned. As such, the Client would not be aware of any errors (like e.g. "Invalid
 * params","Internal error").
 *
 */
using JsonRpcIdType = std::variant<std::nullptr_t, uint64_t, std::string>;
template <typename T>
struct JsonRpcRequest2;
template <typename... Args>
struct JsonRpcRequest2<traits::RpcMethodTraits<Args...>> {
    std::optional<std::string> jsonrpc = "2.0";
    std::string method;
    traits::RpcMethodTraits<Args...>::ParamsTupleType params;
    std::optional<JsonRpcIdType> id;

    NEKO_SERIALIZER(jsonrpc, method, params, id)
};

struct JsonRpcRequestMethod {
    std::optional<std::string> jsonrpc;
    std::string method;
    std::optional<JsonRpcIdType> id;
    NEKO_SERIALIZER(jsonrpc, method, id)
};

struct JsonRpcErrorResponse {
    int64_t code;
    std::string message;
    // 该字段内容由服务器自定义
    std::map<std::string, std::string> data;
    NEKO_SERIALIZER(code, message, data)
};

template <typename T>
struct JsonRpcResponse;
template <typename... Args>
struct JsonRpcResponse<traits::RpcMethodTraits<Args...>> {
    using ReturnType = typename traits::RpcMethodTraits<Args...>::ReturnType;

    std::string jsonrpc = "2.0";
    ReturnType result;
    std::optional<JsonRpcErrorResponse> error;
    JsonRpcIdType id;

    NEKO_SERIALIZER(jsonrpc, result, error, id)
};

template <>
struct JsonRpcResponse<void> {
    std::string jsonrpc = "2.0";
    std::optional<JsonRpcErrorResponse> error;
    JsonRpcIdType id;

    NEKO_SERIALIZER(jsonrpc, error, id)
};

class JsonRpcServerImp {
public:
    JsonRpcServerImp() {}

    template <typename T>
    auto registerRpcMethod(T& metadata) -> void {
        if (auto item = mHandlers.find(T::Name); item != mHandlers.end()) {
            NEKO_LOG_ERROR("jsonrpc", "Method {} exist!!!", T::Name);
            return;
        }
        mHandlers[T::Name] = [&metadata, this](NekoProto::JsonSerializer::InputSerializer& in,
                                               NekoProto::JsonSerializer::OutputSerializer& out,
                                               JsonRpcRequestMethod& method) -> ILIAS_NAMESPACE::Task<void> {
            return _processRequest(in, out, method, metadata);
        };
    }

    auto processRequest(const char* data, std::size_t size, DatagramClientBase* client)
        -> ILIAS_NAMESPACE::Task<std::vector<char>> {
        std::vector<char> buffer;
        NekoProto::JsonSerializer::OutputSerializer out(buffer);
        NekoProto::JsonSerializer::InputSerializer in(data, size);
        while ((*data == '\n' || *data == ' ') && size > 0) {
            data++;
            size--;
        }
        bool batchRequest     = false;
        std::size_t batchSize = 1;
        if (data[0] == '[') { // batch request
            batchRequest = true;
            in.startNode();
            in(make_size_tag(batchSize));
            out.startArray(batchSize);
        }
        co_await _makeBatch(in, out, batchSize);
        if (batchRequest) {
            in.finishNode();
            out.endArray();
        }
        if (batchSize > 0) {
            out.end();
        }
        if (client != nullptr) {
            auto ret = co_await client->send({reinterpret_cast<std::byte*>(buffer.data()), buffer.size()});
            if (ret.error_or(ILIAS_NAMESPACE::Error::Unknown) == JsonRpcError::MessageToolLarge) {
                NEKO_LOG_ERROR("jsonrpc", "message too large!");
            }
        }
        co_return buffer;
    }

    auto receiveLoop(DatagramClientBase* client) -> ILIAS_NAMESPACE::Task<void> {
        while (client != nullptr) {
            if (auto buffer = co_await client->recv(); buffer && buffer->size() > 0) {
                co_await processRequest(reinterpret_cast<const char*>(buffer->data()), buffer->size(), client);
            } else {
                NEKO_LOG_WARN("jsonrpc", "client disconnected, {}",
                              buffer.error_or(ILIAS_NAMESPACE::Error::ConnectionReset).message());
                break;
            }
        }
    }

private:
    auto _makeBatch(NekoProto::JsonSerializer::InputSerializer& in, NekoProto::JsonSerializer::OutputSerializer& out,
                    int batchSize) -> ILIAS_NAMESPACE::Task<void> {
        std::vector<ILIAS_NAMESPACE::Task<>> tasks;
        for (int ix = 0; ix < batchSize; ++ix) {
            JsonRpcRequestMethod method;
            if (!in(method)) {
                break;
            }
            if (method.jsonrpc.has_value() && method.jsonrpc.value() != "2.0") {
                NEKO_LOG_ERROR("jsonrpc", "unsupport jsonrpc version! {}", method.jsonrpc.value());
                break;
            }
            in.rollbackItem();
            if (auto it = mHandlers.find(method.method); it != mHandlers.end()) {
                tasks.emplace_back(it->second(in, out, method));
            }
        }
        co_await ILIAS_NAMESPACE::whenAll(std::move(tasks));
        co_return;
    }

    template <typename T, typename RetT>
    auto _handleSuccess(NekoProto::JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod method, RetT&& ret)
        -> ILIAS_NAMESPACE::Task<void> {
        if (!method.id.has_value()) { // notification
            co_return;
        }
        if (!method.jsonrpc.has_value() && method.id.value().index() == 0) { // notification in jsonrpc 1.0
            co_return;
        }
        std::vector<char> buffer;
        if constexpr (std::is_void_v<typename T::RawReturnType>) {
            if (!out(T::response(method.id.value(), nullptr))) {
                NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
                co_return;
            }
        } else {
            if (!out(T::response(method.id.value(), std::forward<RetT>(ret)))) {
                NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
                co_return;
            }
        }
    }

    template <typename T>
    auto _handleError(NekoProto::JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod method,
                      JsonRpcErrorResponse&& error) -> ILIAS_NAMESPACE::Task<void> {
        if (!method.id.has_value()) { // notification
            co_return;
        }
        if (!method.jsonrpc.has_value() && method.id.value().index() == 0) { // notification in jsonrpc 1.0
            co_return;
        }
        if (!out(T::response(method.id.value(), error))) {
            NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
            co_return;
        }
    }

    template <typename T>
    auto _callMethod(typename T::RequestType request, NekoProto::JsonSerializer::OutputSerializer& out,
                     JsonRpcRequestMethod method, T& metadata) -> ILIAS_NAMESPACE::Task<void> {
        if constexpr (std::is_void_v<typename T::RawReturnType>) {
            if constexpr (NekoProto::traits::is_optional<typename T::ParamsTupleType>::value) {
                if (auto ret = co_await metadata(); ret) {
                    co_return co_await _handleSuccess<T, std::nullptr_t>(out, std::move(method), nullptr);
                } else {
                    NEKO_LOG_ERROR("jsonrpc", "method {} failed to execute, {}", method.method, ret.error().message());
                    co_return co_await _handleError<T>(
                        out, std::move(method), JsonRpcErrorResponse{ret.error().value(), ret.error().message()});
                }
            } else {
                if (auto ret = co_await std::apply(metadata, request.params); ret) {
                    co_return co_await _handleSuccess<T, std::nullptr_t>(out, std::move(method), nullptr);
                } else {
                    NEKO_LOG_ERROR("jsonrpc", "method {} failed to execute, {}", method.method, ret.error().message());
                    co_return co_await _handleError<T>(
                        out, std::move(method), JsonRpcErrorResponse{ret.error().value(), ret.error().message()});
                }
            }
        } else {
            if constexpr (NekoProto::traits::is_optional<typename T::ParamsTupleType>::value) {
                if (auto ret = co_await metadata(); ret) {
                    co_return co_await _handleSuccess<T, typename T::RawReturnType>(out, std::move(method),
                                                                                    std::move(ret.value()));
                } else {
                    NEKO_LOG_ERROR("jsonrpc", "method {} failed to execute, {}", method.method, ret.error().message());
                    co_return co_await _handleError<T>(
                        out, std::move(method), JsonRpcErrorResponse{ret.error().value(), ret.error().message()});
                }
            } else {
                if (auto ret = co_await std::apply(metadata, request.params); ret) {
                    co_return co_await _handleSuccess<T, typename T::RawReturnType>(out, std::move(method),
                                                                                    std::move(ret.value()));
                } else {
                    NEKO_LOG_ERROR("jsonrpc", "method {} failed to execute, {}", method.method, ret.error().message());
                    co_return co_await _handleError<T>(
                        out, std::move(method), JsonRpcErrorResponse{ret.error().value(), ret.error().message()});
                }
            }
        }
    }
    template <typename T>
    auto _processRequest(NekoProto::JsonSerializer::InputSerializer& in,
                         NekoProto::JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod& method, T& metadata)
        -> ILIAS_NAMESPACE::Task<void> {
        typename T::RequestType request;
        if (!in(request)) {
            NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc request");
            return _handleError<T>(out, std::move(method), JsonRpcErrorResponse{-32600, "Invalid Request", {}});
        }
        return _callMethod(std::move(request), out, std::move(method), metadata);
    }

private:
    std::map<std::string_view, std::function<ILIAS_NAMESPACE::Task<void>(
                                   NekoProto::JsonSerializer::InputSerializer& in,
                                   NekoProto::JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod& method)>>
        mHandlers;
};

} // namespace traits

template <traits::RpcMethod T, traits::MethodNameString MethodName>
class RpcMethod : public traits::RpcMethodTraits<T> {
public:
    using MethodType   = T;
    using MethodTraits = traits::RpcMethodTraits<T>;
    using typename MethodTraits::CoroutinesFuncType;
    using typename MethodTraits::FunctionType;
    using typename MethodTraits::ParamsTupleType;
    using typename MethodTraits::RawParamsType;
    using typename MethodTraits::RawReturnType;
    using typename MethodTraits::ReturnType;
    using RequestType  = traits::JsonRpcRequest2<MethodTraits>;
    using ResponseType = traits::JsonRpcResponse<MethodTraits>;

    static constexpr std::string_view Name = MethodName.view();

    static RequestType request(const traits::JsonRpcIdType& id, ParamsTupleType&& params = {}) {
        return RequestType{.method = MethodName.c_str(), .params = std::forward<ParamsTupleType>(params), .id = id};
    }

    static ResponseType response(const traits::JsonRpcIdType& id, const MethodTraits::ReturnType& result) {
        return ResponseType{.result = result, .error = {}, .id = id};
    }

    static ResponseType response(const traits::JsonRpcIdType& id, const traits::JsonRpcErrorResponse& error) {
        return ResponseType{.result = {}, .error = error, .id = id};
    }

    RpcMethod operator=(const FunctionType& func) {
        this->mCoFunction = [func](auto... args) -> ILIAS_NAMESPACE::IoTask<RawReturnType> {
            if constexpr (std::is_void_v<RawReturnType>) {
                func(args...);
                co_return {};
            } else {
                co_return func(args...);
            }
        };
        return *this;
    }

    RpcMethod operator=(const CoroutinesFuncType& func) {
        this->mCoFunction = func;
        return *this;
    }
};

template <typename ProtocolT>
class JsonRpcServer {
public:
    JsonRpcServer(ILIAS_NAMESPACE::IoContext& ctx) : mScop(ctx) {
        auto rpcMethodMetadatas = detail::unwrap_struct(mProtocol);
        for_each_tuple_element(rpcMethodMetadatas,
                               [this](auto&& rpcMethodMetadata) { mImp.registerRpcMethod(rpcMethodMetadata); });
        mScop.setAutoCancel(true);
    }
    ~JsonRpcServer() { close(); }
    auto operator->() { return &mProtocol; }
    auto operator->() const { return &mProtocol; }
    auto close() -> void {
        for (auto& client : mClients) {
            dynamic_cast<DatagramBase*>(client.get())->close();
        }
        mScop.cancel();
        mScop.wait();
        if (mServer != nullptr) {
            dynamic_cast<DatagramBase*>(mServer.get())->close();
        }
    }

    auto wait() -> ILIAS_NAMESPACE::Task<void> { co_await mScop.wait(); }

    template <typename StreamType>
    auto start(std::string_view url) -> ILIAS_NAMESPACE::Task<bool> {
        auto server = std::make_unique<DatagramClient<StreamType>>();
        if (server->checkProtocol(DatagramClient<StreamType>::Type::Server, url)) {
            if (auto ret = co_await server->start(url); ret) {
                mServer = std::move(server);
                mScop.spawn(_acceptLoop());
                co_return true;
            } else {
                NEKO_LOG_ERROR("jsonrpc", "client start failed, {}", ret.error().message());
                co_return false;
            }
        } else {
            co_return false;
        }
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::Task<bool> {
        if (auto ret = co_await start<ILIAS_NAMESPACE::TcpListener>(url); ret) {
            co_return true;
        }
        if (auto ret = co_await start<ILIAS_NAMESPACE::UdpClient>(url); ret) {
            co_return true;
        }
        NEKO_LOG_ERROR("jsonrpc", "Unsupported protocol: {}", url);
        co_return false;
    }

    auto callMethod(std::string_view json) -> ILIAS_NAMESPACE::Task<std::vector<char>> {
        co_return co_await mImp.processRequest(json.data(), json.size(), nullptr);
    }

private:
    auto _acceptLoop() -> ILIAS_NAMESPACE::Task<> {
        while (mServer != nullptr) {
            if (auto ret = co_await mServer->accept(); ret) {
                if (mClients.back() == ret.value()) {
                    break;
                }
                auto item = mClients.emplace(mClients.end(), std::move(ret.value()));
                mScop.spawn([this, item]() -> ILIAS_NAMESPACE::Task<void> {
                    co_await mImp.receiveLoop((*item).get());
                    mClients.erase(item);
                });
            } else {
                NEKO_LOG_WARN("jsonrpc", "accepting exit wit: {}", ret.error());
                break;
            }
        }
    }

private:
    ProtocolT mProtocol;
    traits::JsonRpcServerImp mImp;
    std::unique_ptr<DatagramServerBase> mServer;
    std::list<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> mClients;
    ILIAS_NAMESPACE::TaskScope mScop;
};

template <typename ProtocolT>
class JsonRpcClient {
public:
    JsonRpcClient(ILIAS_NAMESPACE::IoContext& ctx) : mScop(ctx) {
        auto rpcMethodMetadatas = detail::unwrap_struct(mProtocol);
        for_each_tuple_element(rpcMethodMetadatas,
                               [this](auto&& rpcMethodMetadata) { _registerRpcMethod(rpcMethodMetadata); });
        mScop.setAutoCancel(true);
    }
    ~JsonRpcClient() { close(); }
    auto operator->() const { return &mProtocol; }
    auto close() -> void {
        if (mClient != nullptr) {
            dynamic_cast<DatagramBase*>(mClient.get())->close();
        }
        mScop.cancel();
        mScop.wait();
    }

    template <typename StreamType>
    auto connect(std::string_view url) -> ILIAS_NAMESPACE::Task<bool> {
        auto client = std::make_unique<DatagramClient<StreamType>>();
        if (client->checkProtocol(DatagramClient<StreamType>::Type::Client, url)) {
            if (auto ret = co_await client->start(url); ret) {
                mClient = std::move(client);
                co_return true;
            } else {
                NEKO_LOG_ERROR("jsonrpc", "client start failed, {}", ret.error().message());
                co_return false;
            }
        } else {
            co_return false;
        }
    }

    auto connect(std::string_view url) -> ILIAS_NAMESPACE::Task<bool> {
        if (auto ret = co_await connect<ILIAS_NAMESPACE::TcpClient>(url); ret) {
            co_return true;
        }
        if (auto ret = co_await connect<ILIAS_NAMESPACE::UdpClient>(url); ret) {
            co_return true;
        }
        NEKO_LOG_ERROR("jsonrpc", "Unsupported protocol: {}", url);
        co_return false;
    }

private:
    template <typename T>
    void _registerRpcMethod(T&& metadata) {
        metadata = (typename std::decay_t<T>::CoroutinesFuncType)[this, &metadata](auto... args)
                       ->ILIAS_NAMESPACE::IoTask<typename std::decay_t<T>::RawReturnType> {
            auto waithandle = mScop.spawn(_callRemote<T, decltype(args)...>(metadata, args...));
            co_return co_await std::move(waithandle);
        };
    }

    template <typename T, typename... Args>
    auto _callRemote(T& metadata, Args... args) -> ILIAS_NAMESPACE::IoTask<typename std::decay_t<T>::RawReturnType> {
        if (mClient == nullptr) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (auto grid = co_await mMutex.uniqueLock(); grid) {
            typename std::decay_t<T>::RequestType request;
            if (auto ret =
                    co_await _sendRequest<T, Args...>(metadata.isNotification(), request, std::forward<Args>(args)...);
                !ret) {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
            if (!metadata.isNotification()) {
                typename std::decay_t<T>::ResponseType respone;
                if (auto ret = co_await _recvResponse<T>(respone, request.id.value()); !ret) {
                    co_return ILIAS_NAMESPACE::Unexpected(ret.error());
                } else {
                    if constexpr (std::is_void_v<typename std::decay_t<T>::RawReturnType>) {
                        co_return {};
                    } else {
                        co_return ret.value();
                    }
                }
            }
        } else {
            NEKO_LOG_ERROR("jsonrpc", "Error: {}", grid.error().message());
            co_return ILIAS_NAMESPACE::Unexpected(grid.error());
        }
        co_return {};
    }

    template <typename T, typename... Args>
    auto _sendRequest(bool notification, typename std::decay_t<T>::RequestType& request, Args... args)
        -> ILIAS_NAMESPACE::IoTask<void> {
        if constexpr (NekoProto::traits::is_optional<typename std::decay_t<T>::ParamsTupleType>::value) {
            if (notification) {
                request    = std::decay_t<T>::request(nullptr);
                request.id = std::nullopt;
            } else {
                request = std::decay_t<T>::request(mId++);
            }
        } else {
            if (notification) {
                request    = std::decay_t<T>::request(nullptr, std::forward_as_tuple(args...));
                request.id = std::nullopt;
            } else {
                request = std::decay_t<T>::request(mId++, std::forward_as_tuple(args...));
            }
        }
        mBuffer.clear();
        if (auto out = NekoProto::JsonSerializer::OutputSerializer(mBuffer); out(request)) {
            NEKO_LOG_INFO("jsonrpc", "send {}: {}", notification ? "notification" : "request",
                          std::string_view{mBuffer.data(), mBuffer.size()});
            if (auto ret = co_await mClient->send({reinterpret_cast<std::byte*>(mBuffer.data()), mBuffer.size()});
                !ret) {
                NEKO_LOG_ERROR("jsonrpc", "send {} request failed", request.method);
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
        } else {
            NEKO_LOG_ERROR("jsonrpc", "make {} request failed", request.method);
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::InvalidRequest);
        }
        co_return {};
    }

    template <typename T>
    auto _recvResponse(typename std::decay_t<T>::ResponseType& response, traits::JsonRpcIdType& id)
        -> ILIAS_NAMESPACE::IoTask<typename std::decay_t<T>::RawReturnType> {
        if (auto ret = co_await mClient->recv(); ret) {
            auto buffer = ret.value();
            NEKO_LOG_INFO("jsonrpc", "recv response: {}",
                          std::string_view{reinterpret_cast<const char*>(buffer.data()), buffer.size()});
            if (auto in = NekoProto::JsonSerializer::InputSerializer(reinterpret_cast<const char*>(buffer.data()),
                                                                     buffer.size());
                in(response)) {
                if (response.id != id) {
                    NEKO_LOG_ERROR("jsonrpc", "id mismatch: {} != {}", std::get<uint64_t>(response.id),
                                   std::get<uint64_t>(id));
                    co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ResponseIdNotMatch);
                }
                if (response.error.has_value()) {
                    traits::JsonRpcErrorResponse err = response.error.value();
                    NEKO_LOG_ERROR("jsonrpc", "Error({}): {}", err.code, err.message);
                    co_return ILIAS_NAMESPACE::Unexpected(
                        err.code > 0 ? ILIAS_NAMESPACE::Error(ILIAS_NAMESPACE::Error::Code(err.code))
                                     : ILIAS_NAMESPACE::Error(JsonRpcError(err.code)));
                } else {
                    if constexpr (std::is_void_v<typename std::decay_t<T>::RawReturnType>) {
                        co_return {};
                    } else {
                        co_return response.result.value();
                    }
                }
            } else {
                NEKO_LOG_ERROR("jsonrpc", "Error: response parser error");
                co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ParseError);
            }
        } else {
            NEKO_LOG_ERROR("jsonrpc", "Error: {}", ret.error().message());
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
    }

private:
    ProtocolT mProtocol;
    std::unique_ptr<DatagramClientBase> mClient = nullptr;
    uint64_t mId                                = 0;
    std::vector<char> mBuffer;
    ILIAS_NAMESPACE::Mutex mMutex;
    ILIAS_NAMESPACE::TaskScope mScop;
};
NEKO_END_NAMESPACE
