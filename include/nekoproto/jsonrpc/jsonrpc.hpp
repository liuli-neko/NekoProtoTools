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

#include "nekoproto/jsonrpc/datagram_wapper.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/jsonrpc/jsonrpc_traits.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {
template <typename T, class enable = void>
class RpcMethodTraits;

template <typename RetT, typename... Args>
    requires traits::IsSerializable<std::tuple<std::remove_cvref_t<Args>...>>::value &&
             traits::IsSerializable<std::remove_cvref_t<RetT>>::value
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

    auto notification(Args... args) const -> ILIAS_NAMESPACE::IoTask<void> {
        std::unique_ptr<bool, std::function<void(bool*)>> raii((bool*)(mIsNotification = true),
                                                               [this](bool*) { mIsNotification = false; });
        if (mCoFunction) {
            if (auto ret = co_await mCoFunction(std::forward<Args>(args)...); !ret) {
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

template <typename RetT>
    requires traits::IsSerializable<std::remove_cvref_t<RetT>>::value
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
concept RpcMethodT = requires() { std::is_constructible_v<typename RpcMethodTraits<T>::FunctionType, T>; };

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
struct JsonRpcRequest2<RpcMethodTraits<Args...>> {
    std::optional<std::string> jsonrpc = "2.0";
    std::string method;
    RpcMethodTraits<Args...>::ParamsTupleType params;
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
struct JsonRpcResponse<RpcMethodTraits<Args...>> {
    using ReturnType = typename RpcMethodTraits<Args...>::ReturnType;

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

template <RpcMethodT T, traits::MethodNameString MethodName>
class RpcMethod : public RpcMethodTraits<T> {
public:
    using MethodType   = T;
    using MethodTraits = RpcMethodTraits<T>;
    using typename MethodTraits::CoroutinesFuncType;
    using typename MethodTraits::FunctionType;
    using typename MethodTraits::ParamsTupleType;
    using typename MethodTraits::RawParamsType;
    using typename MethodTraits::RawReturnType;
    using typename MethodTraits::ReturnType;
    using RequestType  = JsonRpcRequest2<MethodTraits>;
    using ResponseType = JsonRpcResponse<MethodTraits>;

    static constexpr std::string_view Name = MethodName.view();

    static RequestType request(const JsonRpcIdType& id, ParamsTupleType&& params = {}) {
        return RequestType{.method = MethodName.c_str(), .params = std::forward<ParamsTupleType>(params), .id = id};
    }

    static ResponseType response(const JsonRpcIdType& id, const MethodTraits::ReturnType& result) {
        return ResponseType{.result = result, .error = {}, .id = id};
    }

    static ResponseType response(const JsonRpcIdType& id, const JsonRpcErrorResponse& error) {
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

    void clear() { this->mCoFunction = nullptr; }

    operator bool() const { return this->mCoFunction != nullptr; }

    std::string description = []() {
        return traits::TypeName<RawReturnType>::name() + " " + std::string(Name) + "(" +
               traits::parameter_to_string<RawParamsType>(
                   std::make_index_sequence<std::tuple_size_v<RawParamsType>>{}) +
               ")";
    }();
};

template <RpcMethodT T>
class RpcMethodDynamic : public RpcMethodTraits<T> {
public:
    using MethodType   = T;
    using MethodTraits = RpcMethodTraits<T>;
    using typename MethodTraits::CoroutinesFuncType;
    using typename MethodTraits::FunctionType;
    using typename MethodTraits::ParamsTupleType;
    using typename MethodTraits::RawParamsType;
    using typename MethodTraits::RawReturnType;
    using typename MethodTraits::ReturnType;
    using RequestType  = JsonRpcRequest2<MethodTraits>;
    using ResponseType = JsonRpcResponse<MethodTraits>;

    RpcMethodDynamic(std::string_view name, FunctionType func, bool isNotification = false) {
        gName             = name;
        this->mCoFunction = [func](auto... args) -> ILIAS_NAMESPACE::IoTask<RawReturnType> {
            if constexpr (std::is_void_v<RawReturnType>) {
                func(args...);
                co_return {};
            } else {
                co_return func(args...);
            }
        };
        this->mIsNotification = isNotification;
    }

    RpcMethodDynamic(std::string_view name, CoroutinesFuncType func, bool isNotification = false) {
        gName                 = name;
        this->mCoFunction     = func;
        this->mIsNotification = isNotification;
    }

    static RequestType request(const JsonRpcIdType& id, ParamsTupleType&& params = {}) {
        return RequestType{.method = gName.c_str(), .params = std::forward<ParamsTupleType>(params), .id = id};
    }

    static ResponseType response(const JsonRpcIdType& id, const MethodTraits::ReturnType& result) {
        return ResponseType{.result = result, .error = {}, .id = id};
    }

    static ResponseType response(const JsonRpcIdType& id, const JsonRpcErrorResponse& error) {
        return ResponseType{.result = {}, .error = error, .id = id};
    }

    RpcMethodDynamic operator=(const FunctionType& func) {
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

    RpcMethodDynamic operator=(const CoroutinesFuncType& func) {
        this->mCoFunction = func;
        return *this;
    }

    void clear() { this->mCoFunction = nullptr; }

private:
    static std::string gName;
};

template <RpcMethodT T>
std::string RpcMethodDynamic<T>::gName;

struct RpcMethodErrorHelper {
    using ResponseType = JsonRpcResponse<RpcMethodTraits<void(void)>>;
    static ResponseType response(const JsonRpcIdType& id, const JsonRpcErrorResponse& error) {
        return ResponseType{.result = {}, .error = error, .id = id};
    }
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

    template <typename RetT, typename... Args>
    auto bindRpcMethod(std::string_view name, std::function<RetT(Args...)> func) -> void {
        mHandlers[name] = [metadata = RpcMethodDynamic<RetT(Args...)>(name, func),
                           this](NekoProto::JsonSerializer::InputSerializer& in,
                                 NekoProto::JsonSerializer::OutputSerializer& out,
                                 JsonRpcRequestMethod& method) -> ILIAS_NAMESPACE::Task<void> {
            return _processRequest(in, out, method, metadata);
        };
    }

    template <typename RetT, typename... Args>
    auto bindRpcMethod(std::string_view name, std::function<ILIAS_NAMESPACE::IoTask<RetT>(Args...)> func) -> void {
        mHandlers[name] = [metadata = RpcMethodDynamic<RetT(Args...)>(name, func),
                           this](NekoProto::JsonSerializer::InputSerializer& in,
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
        co_await _makeBatch(in, out, (int)batchSize);
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
                if (buffer.error_or(ILIAS_NAMESPACE::Error::Unknown) != ILIAS_NAMESPACE::Error::Canceled) {
                    NEKO_LOG_WARN("jsonrpc", "client disconnected, {}",
                                  buffer.error_or(ILIAS_NAMESPACE::Error::ConnectionReset).message());
                }
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
            } else {
                NEKO_LOG_WARN("jsonrpc", "method {} not found!", method.method);
                JsonRpcErrorResponse error{(int)JsonRpcError::MethodNotFound,
                                           JsonRpcErrorCategory::instance().message((int)JsonRpcError::MethodNotFound),
                                           {}};
                tasks.emplace_back(_handleError<RpcMethodErrorHelper>(out, std::move(method), std::move(error)));
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
                    NEKO_LOG_WARN("jsonrpc", "method {} failed to execute, {}", method.method, ret.error().message());
                    co_return co_await _handleError<T>(
                        out, std::move(method), JsonRpcErrorResponse{ret.error().value(), ret.error().message()});
                }
            } else {
                if (auto ret = co_await std::apply(metadata, request.params); ret) {
                    co_return co_await _handleSuccess<T, typename T::RawReturnType>(out, std::move(method),
                                                                                    std::move(ret.value()));
                } else {
                    NEKO_LOG_WARN("jsonrpc", "method {} failed to execute, {}", method.method, ret.error().message());
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

} // namespace detail

using detail::RpcMethod;
template <typename ProtocolT>
class JsonRpcServer {
private:
    struct MethodMetadataBase {
        virtual ~MethodMetadataBase()          = default;
        virtual std::string_view description() = 0;
        virtual bool isBinded() const { return true; }
    };
    struct {
        RpcMethod<std::vector<std::string>(), "rpc.get_method_list"> getMethodList;
        RpcMethod<std::vector<std::string>(), "rpc.get_method_info_list"> getMethodInfoList;
        RpcMethod<std::string(std::string), "rpc.get_method_info"> getMethodInfo;
        RpcMethod<std::vector<std::string>(), "rpc.get_bind_method_list"> getBindedMethodList;
    } mRpc;

public:
    JsonRpcServer(ILIAS_NAMESPACE::IoContext& ctx) : mScop(ctx) {
        mScop.setAutoCancel(true);
        _init();
    }
    ~JsonRpcServer() { close(); }
    auto operator->() { return &mProtocol; }
    auto operator->() const { return &mProtocol; }
    auto close() -> void {
        for (auto& client : mClients) {
            dynamic_cast<DatagramBase*>(client.get())->cancel();
        }
        if (mServer != nullptr) {
            dynamic_cast<DatagramBase*>(mServer.get())->cancel();
        }
        mScop.cancel();
        mScop.wait();
        for (auto& client : mClients) {
            dynamic_cast<DatagramBase*>(client.get())->close();
        }
        if (mServer != nullptr) {
            dynamic_cast<DatagramBase*>(mServer.get())->close();
        }
    }
    auto isListening() const -> bool { return mServer != nullptr && mServer->isListening(); }

    auto wait() -> ILIAS_NAMESPACE::Task<void> { co_await mScop.wait(); }

    template <typename StreamType>
    auto start(std::string_view url) -> ILIAS_NAMESPACE::IoTask<ILIAS_NAMESPACE::Error> {
        auto server = std::make_unique<DatagramClient<StreamType>>();
        if (server->checkProtocol(DatagramClient<StreamType>::Type::Server, url)) {
            if (auto ret = co_await server->start(url); ret) {
                mServer = std::move(server);
                mScop.spawn(_acceptLoop());
                co_return ILIAS_NAMESPACE::Error::Ok;
            } else {
                NEKO_LOG_ERROR("jsonrpc", "client start failed, {}", ret.error().message());
                co_return ret.error();
            }
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
    }

    auto start(std::string_view url) -> ILIAS_NAMESPACE::Task<bool> {
        if (auto ret = co_await start<ILIAS_NAMESPACE::TcpListener>(url); ret) {
            co_return ret.value() == ILIAS_NAMESPACE::Error::Ok;
        }
        if (auto ret = co_await start<ILIAS_NAMESPACE::UdpClient>(url); ret) {
            co_return ret.value() == ILIAS_NAMESPACE::Error::Ok;
        }
        NEKO_LOG_ERROR("jsonrpc", "Unsupported protocol: {}", url);
        co_return false;
    }

    template <typename RetT, typename... Args>
    auto bindMethod(std::string_view name, std::function<RetT(Args...)> func) -> void {
        mImp.bindRpcMethod(name, func);
        class MethodMetadata : public MethodMetadataBase {
        public:
            std::string_view description() override { return descript; }
            std::string descript;
        };
        auto metadata = std::make_unique<MethodMetadata>();
        metadata->descript =
            traits::TypeName<RetT>::name() + " " + std::string(name) + "(" +
            traits::parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}) + ")";
        mMethodMetadatas[name] = std::move(metadata);
    }

    template <typename RetT, typename... Args>
    auto bindMethod(std::string_view name, std::function<ILIAS_NAMESPACE::IoTask<RetT>(Args...)> func) -> void {
        mImp.bindRpcMethod(name, func);
        class MethodMetadata : public MethodMetadataBase {
        public:
            std::string_view description() override { return descript; }
            std::string descript;
        };
        auto metadata = std::make_unique<MethodMetadata>();
        metadata->descript =
            traits::TypeName<RetT>::name() + " " + std::string(name) + "(" +
            traits::parameter_to_string<std::tuple<Args...>>(std::make_index_sequence<sizeof...(Args)>{}) + ")";
        mMethodMetadatas[name] = std::move(metadata);
    }

    auto callMethod(std::string_view json) -> ILIAS_NAMESPACE::Task<std::vector<char>> {
        co_return co_await mImp.processRequest(json.data(), json.size(), nullptr);
    }

    auto methodMetadatas() -> std::map<std::string_view, std::unique_ptr<MethodMetadataBase>>& {
        return mMethodMetadatas;
    }

private:
    auto _acceptLoop() -> ILIAS_NAMESPACE::Task<> {
        while (mServer != nullptr) {
            if (auto ret = co_await mServer->accept(); ret) {
                if (mClients.size() > 0 && mClients.back() == ret.value()) {
                    break;
                }
                auto item = mClients.emplace(mClients.end(), std::move(ret.value()));
                mScop.spawn([this, item]() -> ILIAS_NAMESPACE::Task<void> {
                    co_await mImp.receiveLoop((*item).get());
                    mClients.erase(item);
                });
            } else {
                if (ret.error() != ILIAS_NAMESPACE::Error::Canceled) {
                    NEKO_LOG_WARN("jsonrpc", "accepting exit wit: {}", ret.error().message());
                }
                break;
            }
        }
    }

    auto _init() {
        auto rpcMethodMetadatas = detail::unwrap_struct(mProtocol);
        auto registerRpcMethod  = [this]<typename T>(T& rpcMethodMetadata) {
            struct MethodMetadata : public MethodMetadataBase {
                std::string_view description() override { return rawData->description; }
                bool isBinded() const { return static_cast<bool>(*rawData); }
                T* rawData;
            };
            auto metadata                            = std::make_unique<MethodMetadata>();
            metadata->rawData                        = std::addressof(rpcMethodMetadata);
            mMethodMetadatas[rpcMethodMetadata.Name] = std::move(metadata);
            mImp.registerRpcMethod(rpcMethodMetadata);
        };
        traits::for_each_tuple_element(rpcMethodMetadatas, registerRpcMethod);
        auto rpcMethoddatas = detail::unwrap_struct(mRpc);
        traits::for_each_tuple_element(rpcMethoddatas, registerRpcMethod);
        mRpc.getMethodInfo = [this](std::string methodName) -> std::string {
            if (auto it = methodMetadatas().find(methodName); it != methodMetadatas().end()) {
                return std::string(it->second->description());
            }
            return std::string("Method not found!");
        };
        mRpc.getMethodInfoList = [this]() -> std::vector<std::string> {
            std::vector<std::string> methodDesList;
            for (auto& item : methodMetadatas()) {
                methodDesList.emplace_back(item.second->description());
            }
            return methodDesList;
        };
        mRpc.getMethodList = [this]() -> std::vector<std::string> {
            std::vector<std::string> methodList;
            for (auto& item : methodMetadatas()) {
                methodList.emplace_back(item.first);
            }
            return methodList;
        };
        mRpc.getBindedMethodList = [this]() -> std::vector<std::string> {
            std::vector<std::string> methodList;
            for (auto& item : methodMetadatas()) {
                if (item.second->isBinded()) {
                    methodList.emplace_back(item.first);
                }
            }
            return methodList;
        };
    }

private:
    ProtocolT mProtocol;
    detail::JsonRpcServerImp mImp;
    std::unique_ptr<DatagramServerBase> mServer;
    std::list<std::unique_ptr<DatagramClientBase, void (*)(DatagramClientBase*)>> mClients;
    std::map<std::string_view, std::unique_ptr<MethodMetadataBase>> mMethodMetadatas;
    ILIAS_NAMESPACE::TaskScope mScop;
};

template <typename ProtocolT>
class JsonRpcClient {
public:
    JsonRpcClient(ILIAS_NAMESPACE::IoContext& ctx) : mScop(ctx) {
        auto rpcMethodMetadatas = detail::unwrap_struct(mProtocol);
        traits::for_each_tuple_element(rpcMethodMetadatas,
                                       [this](auto&& rpcMethodMetadata) { _registerRpcMethod(rpcMethodMetadata); });
        mScop.setAutoCancel(true);
    }
    ~JsonRpcClient() { close(); }
    auto operator->() const { return &mProtocol; }
    auto close() -> void {
        if (mClient != nullptr) {
            dynamic_cast<DatagramBase*>(mClient.get())->cancel();
        }
        mScop.cancel();
        mScop.wait();
        if (mClient != nullptr) {
            dynamic_cast<DatagramBase*>(mClient.get())->close();
        }
    }

    auto wait() -> ILIAS_NAMESPACE::Task<void> { co_await mScop.wait(); }

    auto isConnected() const -> bool { return mClient != nullptr && mClient->isConnected(); }

    template <typename StreamType>
    auto connect(std::string_view url) -> ILIAS_NAMESPACE::IoTask<ILIAS_NAMESPACE::Error> {
        auto client = std::make_unique<DatagramClient<StreamType>>();
        if (client->checkProtocol(DatagramClient<StreamType>::Type::Client, url)) {
            if (auto ret = co_await client->start(url); ret) {
                mClient = std::move(client);
                co_return ILIAS_NAMESPACE::Error::Ok;
            } else {
                NEKO_LOG_ERROR("jsonrpc", "client start failed, {}", ret.error().message());
                co_return ret.error();
            }
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::Error::InvalidArgument);
        }
    }

    auto connect(std::string_view url) -> ILIAS_NAMESPACE::Task<bool> {
        if (auto ret = co_await connect<ILIAS_NAMESPACE::TcpClient>(url); ret) {
            co_return ret == ILIAS_NAMESPACE::Error::Ok;
        }
        if (auto ret = co_await connect<ILIAS_NAMESPACE::UdpClient>(url); ret) {
            co_return ret == ILIAS_NAMESPACE::Error::Ok;
        }
        NEKO_LOG_ERROR("jsonrpc", "Unsupported protocol: {}", url);
        co_return false;
    }

    template <typename RetT, typename... Args>
    auto callRemote(std::string_view name, Args... args) -> ILIAS_NAMESPACE::IoTask<RetT> {
        using CoroutinesFuncType = typename detail::RpcMethodDynamic<RetT(Args...)>::CoroutinesFuncType;
        detail::RpcMethodDynamic<RetT(Args...)> metadata(name, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <typename RetT, typename... Args>
    auto notifyRemote(std::string_view name, Args... args) -> ILIAS_NAMESPACE::IoTask<RetT> {
        using CoroutinesFuncType = typename detail::RpcMethodDynamic<RetT(Args...)>::CoroutinesFuncType;
        detail::RpcMethodDynamic<RetT(Args...)> metadata(name, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
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
            } else {
                co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::Ok);
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
    auto _recvResponse(typename std::decay_t<T>::ResponseType& response, detail::JsonRpcIdType& id)
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
                    detail::JsonRpcErrorResponse err = response.error.value();
                    NEKO_LOG_WARN("jsonrpc", "Error({}): {}", err.code, err.message);
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
