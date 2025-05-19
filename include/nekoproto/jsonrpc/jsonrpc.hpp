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
#include <ilias/task/when_all.hpp>
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
    using FunctionType             = std::function<RetT(Args...)>;
    using RawReturnType            = RetT;
    using RawParamsType            = std::tuple<Args...>;
    using CoroutinesFuncType       = std::function<ILIAS_NAMESPACE::IoTask<RawReturnType>(Args...)>;
    constexpr static int NumParams = sizeof...(Args);

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
    using FunctionType             = std::function<RetT()>;
    using RawReturnType            = RetT;
    using RawParamsType            = std::tuple<>;
    using CoroutinesFuncType       = std::function<ILIAS_NAMESPACE::IoTask<RawReturnType>()>;
    constexpr static int NumParams = 0;

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
template <typename T, traits::ConstexprString... ArgNames>
struct JsonRpcRequest2;
template <typename... Args, traits::ConstexprString... ArgNames>
struct JsonRpcRequest2<RpcMethodTraits<Args...>, ArgNames...> {
    using ParamsTupleType = typename RpcMethodTraits<Args...>::ParamsTupleType;
    static_assert(sizeof...(ArgNames) == 0 || RpcMethodTraits<Args...>::NumParams == sizeof...(ArgNames),
                  "JsonRpcRequest2: The number of parameters and names do not match.");

    std::optional<std::string> jsonrpc = "2.0";
    std::string method;
    ParamsTupleType params;
    std::optional<JsonRpcIdType> id;

    template <typename SerializerT>
    bool save(SerializerT& serializer) const NEKO_NOEXCEPT {
        serializer.startObject(-1);
        if constexpr (sizeof...(ArgNames) == 0 && RpcMethodTraits<Args...>::NumParams == 1) {
            if constexpr (!is_minimal_serializable<
                              std::tuple_element_t<0, typename RpcMethodTraits<Args...>::RawParamsType>>::value) {
                return serializer(NEKO_PROTO_NAME_VALUE_PAIR(jsonrpc), NEKO_PROTO_NAME_VALUE_PAIR(method),
                                  make_name_value_pair("params", std::get<0>(params)), NEKO_PROTO_NAME_VALUE_PAIR(id)) &
                       serializer.endObject();
            }
        }
        traits::SerializerHelperObject<const ParamsTupleType, ArgNames...> mParamsHelper{params};
        return serializer(NEKO_PROTO_NAME_VALUE_PAIR(jsonrpc), NEKO_PROTO_NAME_VALUE_PAIR(method),
                          make_name_value_pair("params", mParamsHelper), NEKO_PROTO_NAME_VALUE_PAIR(id)) &
               serializer.endObject();
    }

    template <typename SerializerT>
    bool load(SerializerT& serializer) NEKO_NOEXCEPT {
        serializer.startNode();
        if constexpr (sizeof...(ArgNames) == 0 && RpcMethodTraits<Args...>::NumParams == 1) {
            if constexpr (!is_minimal_serializable<
                              std::tuple_element_t<0, typename RpcMethodTraits<Args...>::RawParamsType>>::value) {

                return serializer(NEKO_PROTO_NAME_VALUE_PAIR(jsonrpc), NEKO_PROTO_NAME_VALUE_PAIR(method),
                                  make_name_value_pair("params", std::get<0>(params)), NEKO_PROTO_NAME_VALUE_PAIR(id)) &
                       serializer.finishNode();
            }
        }
        traits::SerializerHelperObject<ParamsTupleType, ArgNames...> mParamsHelper{params};
        return serializer(NEKO_PROTO_NAME_VALUE_PAIR(jsonrpc), NEKO_PROTO_NAME_VALUE_PAIR(method),
                          make_name_value_pair("params", mParamsHelper), NEKO_PROTO_NAME_VALUE_PAIR(id)) &
               serializer.finishNode();
        ;
    }
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
    NEKO_SERIALIZER(code, message)
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

template <RpcMethodT T, traits::ConstexprString MethodName, traits::ConstexprString... ArgNames>
class RpcMethod : public RpcMethodTraits<T> {
    static_assert(sizeof...(ArgNames) == 0 || RpcMethodTraits<T>::NumParams == sizeof...(ArgNames),
                  "RpcMethodTraits: The number of parameters and names do not match.");

public:
    using MethodType   = T;
    using MethodTraits = RpcMethodTraits<T>;
    using typename MethodTraits::CoroutinesFuncType;
    using typename MethodTraits::FunctionType;
    using typename MethodTraits::ParamsTupleType;
    using typename MethodTraits::RawParamsType;
    using typename MethodTraits::RawReturnType;
    using typename MethodTraits::ReturnType;
    using RequestType  = JsonRpcRequest2<MethodTraits, ArgNames...>;
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
               traits::parameter_to_string<RawParamsType>(std::make_index_sequence<std::tuple_size_v<RawParamsType>>{},
                                                          traits::ArgNamesHelper<ArgNames...>{}) +
               ")";
    }();
};

template <RpcMethodT T, traits::ConstexprString... ArgNames>
class RpcMethodDynamic : public RpcMethodTraits<T> {
    static_assert(sizeof...(ArgNames) == 0 || RpcMethodTraits<T>::NumParams == sizeof...(ArgNames),
                  "RpcMethodDynamic: The number of parameters and names do not match.");

public:
    using MethodType   = T;
    using MethodTraits = RpcMethodTraits<T>;
    using typename MethodTraits::CoroutinesFuncType;
    using typename MethodTraits::FunctionType;
    using typename MethodTraits::ParamsTupleType;
    using typename MethodTraits::RawParamsType;
    using typename MethodTraits::RawReturnType;
    using typename MethodTraits::ReturnType;
    using RequestType  = JsonRpcRequest2<MethodTraits, ArgNames...>;
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

template <RpcMethodT T, traits::ConstexprString... ArgNames>
std::string RpcMethodDynamic<T, ArgNames...>::gName;

struct RpcMethodErrorHelper {
    using ResponseType = JsonRpcResponse<RpcMethodTraits<void(void)>>;
    static ResponseType response(const JsonRpcIdType& id, const JsonRpcErrorResponse& error) {
        return ResponseType{.result = {}, .error = error, .id = id};
    }
};

class JsonRpcServerImp {
private:
    auto _makeBatch(JsonSerializer::InputSerializer& in, JsonSerializer::OutputSerializer& out, int batchSize) {
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
                tasks.emplace_back(it->second(in, out, std::move(method)));
            } else {
                NEKO_LOG_WARN("jsonrpc", "method {} not found!", method.method);
                _handleError<RpcMethodErrorHelper>(
                    out, std::move(method), (int)JsonRpcError::MethodNotFound,
                    JsonRpcErrorCategory::instance().message((int)JsonRpcError::MethodNotFound));
            }
        }
        return ILIAS_NAMESPACE::whenAll(std::move(tasks));
    }

public:
    JsonRpcServerImp() {}

    template <typename T>
    auto registerRpcMethod(T& metadata) -> void {
        if (auto item = mHandlers.find(T::Name); item != mHandlers.end()) {
            NEKO_LOG_ERROR("jsonrpc", "Method {} exist!!!", T::Name);
            return;
        }
        mHandlers[T::Name] = [&metadata, this](JsonSerializer::InputSerializer& in,
                                               JsonSerializer::OutputSerializer& out,
                                               JsonRpcRequestMethod method) -> ILIAS_NAMESPACE::Task<void> {
            typename T::RequestType request;
            auto success = in(request);
            return _handle<T>(success, request, out, std::move(method), metadata);
        };
    }

    template <typename RetT, typename... Args, traits::ConstexprString... ArgNames>
    auto bindRpcMethod(std::string_view name, std::function<RetT(Args...)> func,
                       traits::ArgNamesHelper<ArgNames...> /*unused*/ = {}) -> void {
        return _registerRpcMethod<RpcMethodDynamic<RetT(Args...), ArgNames...>>(name, func);
    }

    template <typename RetT, typename... Args, traits::ConstexprString... ArgNames>
    auto bindRpcMethod(std::string_view name, std::function<ILIAS_NAMESPACE::IoTask<RetT>(Args...)> func,
                       traits::ArgNamesHelper<ArgNames...> /*unused*/ = {}) -> void {
        _registerRpcMethod<RpcMethodDynamic<RetT(Args...), ArgNames...>>(name, func);
    }

    auto processRequest(const char* data, std::size_t size, DatagramClientBase* client)
        -> ILIAS_NAMESPACE::Task<std::vector<char>> {
        std::vector<char> buffer;
        JsonSerializer::OutputSerializer out(buffer);
        JsonSerializer::InputSerializer in(data, size);
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
    template <typename T>
    auto _handle(bool success, auto request, auto& out, auto method, auto& metadata) -> ILIAS_NAMESPACE::Task<void> {
        if (!success) {
            NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc request");
            co_return _handleError<T>(out, std::move(method), (int64_t)JsonRpcError::InvalidRequest,
                                      JsonRpcErrorCategory::instance().message((int64_t)JsonRpcError::InvalidRequest));
        }
        if constexpr (traits::optional_like_type<typename T::ParamsTupleType>::value) {
            co_return _processMethodReturn<T>(co_await metadata(), out, std::move(method));
        } else {
            co_return _processMethodReturn<T>(co_await std::apply(metadata, request.params), out, std::move(method));
        }
    }
    // RpcMethodDynamic<RetT(Args...), ArgNames...>
    template <typename T, typename Callable>
    auto _registerRpcMethod(std::string_view name, std::function<Callable> func) -> void {
        mHandlers[name] = [metadata = T(name, func), this](JsonSerializer::InputSerializer& in,
                                                           JsonSerializer::OutputSerializer& out,
                                                           JsonRpcRequestMethod method) -> ILIAS_NAMESPACE::Task<void> {
            typename T::RequestType request;
            auto success = in(request);
            return _handle<T>(success, request, out, std::move(method), metadata);
        };
    }

    template <typename T, typename RetT>
    auto _handleSuccess(JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod method, RetT&& ret) -> void {
        if (!method.id.has_value()) { // notification
            return;
        }
        if (!method.jsonrpc.has_value() && method.id.value().index() == 0) { // notification in jsonrpc 1.0
            return;
        }
        std::vector<char> buffer;
        if constexpr (std::is_void_v<typename T::RawReturnType>) {
            if (!out(T::response(method.id.value(), nullptr))) {
                NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
                return;
            }
        } else {
            if (!out(T::response(method.id.value(), std::forward<RetT>(ret)))) {
                NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
                return;
            }
        }
    }

    template <typename T>
    auto _handleError(JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod method, int64_t code,
                      std::string message) -> void {
        if (!method.id.has_value()) { // notification
            return;
        }
        if (!method.jsonrpc.has_value() && method.id.value().index() == 0) { // notification in jsonrpc 1.0
            return;
        }
        if (!out(T::response(method.id.value(), {code, message}))) {
            NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
            return;
        }
    }

    template <typename T>
    auto _processMethodReturn(ILIAS_NAMESPACE::Result<typename T::RawReturnType> result,
                              JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod method) -> void {
        if constexpr (std::is_void_v<typename T::RawReturnType>) {
            if (result) {
                return _handleSuccess<T, std::nullptr_t>(out, std::move(method), nullptr);
            }
            NEKO_LOG_ERROR("jsonrpc", "method {} failed to execute, {}", method.method, result.error().message());
            return _handleError<T>(out, std::move(method), result.error().value(), result.error().message());
        } else {
            if (result) {
                return _handleSuccess<T, typename T::RawReturnType>(out, std::move(method), std::move(result.value()));
            }
            NEKO_LOG_WARN("jsonrpc", "method {} failed to execute, {}", method.method, result.error().message());
            return _handleError<T>(out, std::move(method), result.error().value(), result.error().message());
        }
    }

private:
    std::map<std::string_view, std::function<ILIAS_NAMESPACE::Task<void>(JsonSerializer::InputSerializer& in,
                                                                         JsonSerializer::OutputSerializer& out,
                                                                         JsonRpcRequestMethod method)>>
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
        RpcMethod<std::string(std::string), "rpc.get_method_info", "method_name"> getMethodInfo;
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

    template <traits::ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, std::function<RetT(Args...)> func) -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethod: The number of parameters and names do not match.");
        mImp.bindRpcMethod(name, func, traits::ArgNamesHelper<ArgNames...>{});
        class MethodMetadata : public MethodMetadataBase {
        public:
            std::string_view description() override { return descript; }
            std::string descript;
        };
        auto metadata      = std::make_unique<MethodMetadata>();
        metadata->descript = traits::TypeName<RetT>::name() + " " + std::string(name) + "(" +
                             traits::parameter_to_string<std::tuple<Args...>>(
                                 std::make_index_sequence<sizeof...(Args)>{}, traits::ArgNamesHelper<ArgNames...>{}) +
                             ")";
        mMethodMetadatas[name] = std::move(metadata);
    }

    template <traits::ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, std::function<ILIAS_NAMESPACE::IoTask<RetT>(Args...)> func) -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethod: The number of parameters and names do not match.");
        mImp.bindRpcMethod(name, func, traits::ArgNamesHelper<ArgNames...>{});
        class MethodMetadata : public MethodMetadataBase {
        public:
            std::string_view description() override { return descript; }
            std::string descript;
        };
        auto metadata      = std::make_unique<MethodMetadata>();
        metadata->descript = traits::TypeName<RetT>::name() + " " + std::string(name) + "(" +
                             traits::parameter_to_string<std::tuple<Args...>>(
                                 std::make_index_sequence<sizeof...(Args)>{}, traits::ArgNamesHelper<ArgNames...>{}) +
                             ")";
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
        auto registerRpcMethod = [this]<typename T>(T& rpcMethodMetadata) {
            struct MethodMetadata : public MethodMetadataBase {
                std::string_view description() override { return rawData->description; }
                bool isBinded() const override { return static_cast<bool>(*rawData); }
                T* rawData;
            };
            auto metadata                            = std::make_unique<MethodMetadata>();
            metadata->rawData                        = &rpcMethodMetadata;
            mMethodMetadatas[rpcMethodMetadata.Name] = std::move(metadata);
            mImp.registerRpcMethod(rpcMethodMetadata);
        };
        Reflect<ProtocolT>::forEachWithoutName(mProtocol, registerRpcMethod);
        Reflect<decltype(mRpc)>::forEachWithoutName(mRpc, registerRpcMethod);
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
    JsonRpcClient(ILIAS_NAMESPACE::IoContext& /*unused*/) {
        Reflect<ProtocolT>::forEachWithoutName(
            mProtocol, [this](auto& rpcMethodMetadata) { this->_registerRpcMethod(rpcMethodMetadata); });
    }
    ~JsonRpcClient() { close(); }
    auto operator->() const { return &mProtocol; }
    auto close() -> void {
        if (mClient != nullptr) {
            dynamic_cast<DatagramBase*>(mClient.get())->cancel();
        }
        if (mClient != nullptr) {
            dynamic_cast<DatagramBase*>(mClient.get())->close();
        }
    }

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

    template <typename RetT, traits::ConstexprString... ArgNames, typename... Args>
    auto callRemote(std::string_view name, Args... args) -> ILIAS_NAMESPACE::IoTask<RetT> {
        using CoroutinesFuncType = typename detail::RpcMethodDynamic<RetT(Args...), ArgNames...>::CoroutinesFuncType;
        detail::RpcMethodDynamic<RetT(Args...), ArgNames...> metadata(name, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <typename RetT, traits::ConstexprString... ArgNames, typename... Args>
    auto notifyRemote(std::string_view name, Args... args) -> ILIAS_NAMESPACE::IoTask<RetT> {
        using CoroutinesFuncType = typename detail::RpcMethodDynamic<RetT(Args...), ArgNames...>::CoroutinesFuncType;
        detail::RpcMethodDynamic<RetT(Args...), ArgNames...> metadata(name, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

private:
    template <typename T>
    void _registerRpcMethod(T&& metadata) {
        metadata = (typename std::decay_t<T>::CoroutinesFuncType)[this, &metadata](auto... args)
                       ->ILIAS_NAMESPACE::IoTask<typename std::decay_t<T>::RawReturnType> {
            return _callRemote<T, decltype(args)...>(metadata, args...);
        };
    }

    template <typename T, typename... Args>
    auto _callRemote(T& metadata, Args... args) -> ILIAS_NAMESPACE::IoTask<typename std::decay_t<T>::RawReturnType> {
        if (mClient == nullptr) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        if (auto grid = co_await mMutex.uniqueLock(); grid) {
            typename std::decay_t<T>::RequestType request;
            if (auto ret = _sendRequest<T, Args...>(metadata.isNotification(), request, std::forward<Args>(args)...);
                ret) {
                if (auto ret1 = co_await mClient->send({reinterpret_cast<std::byte*>(mBuffer.data()), mBuffer.size()});
                    !ret1) {
                    NEKO_LOG_ERROR("jsonrpc", "send {} request failed", request.method);
                    co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
                }
            } else {
                co_return ILIAS_NAMESPACE::Unexpected(ret.error());
            }
            if (!metadata.isNotification()) {
                typename std::decay_t<T>::ResponseType respone;
                if (auto ret = co_await mClient->recv(); ret) {
                    if (auto ret1 = _recvResponse<T>(ret.value(), respone, request.id.value()); !ret1) {
                        co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
                    } else {
                        if constexpr (std::is_void_v<typename std::decay_t<T>::RawReturnType>) {
                            co_return {};
                        } else {
                            co_return ret1.value();
                        }
                    }
                } else {
                    NEKO_LOG_ERROR("jsonrpc", "Error: {}", ret.error().message());
                    co_return ILIAS_NAMESPACE::Unexpected(ret.error());
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
        -> ILIAS_NAMESPACE::Result<void> {
        if constexpr (traits::optional_like_type<typename std::decay_t<T>::ParamsTupleType>::value) {
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
        if (auto out = JsonSerializer::OutputSerializer(mBuffer); out(request)) {
            NEKO_LOG_INFO("jsonrpc", "send {}: {}", notification ? "notification" : "request",
                          std::string_view{mBuffer.data(), mBuffer.size()});
            return {};
        }
        NEKO_LOG_ERROR("jsonrpc", "make {} request failed", request.method);
        return ILIAS_NAMESPACE::Unexpected(JsonRpcError::InvalidRequest);
    }

    template <typename T>
    auto _recvResponse(std::span<std::byte> buffer, typename std::decay_t<T>::ResponseType& response,
                       detail::JsonRpcIdType& id) -> ILIAS_NAMESPACE::Result<typename std::decay_t<T>::RawReturnType> {
        NEKO_LOG_INFO("jsonrpc", "recv response: {}",
                      std::string_view{reinterpret_cast<const char*>(buffer.data()), buffer.size()});
        if (auto in = JsonSerializer::InputSerializer(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            in(response)) {
            if (response.id != id) {
                NEKO_LOG_ERROR("jsonrpc", "id mismatch: {} != {}", std::get<uint64_t>(response.id),
                               std::get<uint64_t>(id));
                return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ResponseIdNotMatch);
            }
            if (response.error.has_value()) {
                detail::JsonRpcErrorResponse err = response.error.value();
                NEKO_LOG_WARN("jsonrpc", "Error({}): {}", err.code, err.message);
                return ILIAS_NAMESPACE::Unexpected(err.code > 0
                                                       ? ILIAS_NAMESPACE::Error(ILIAS_NAMESPACE::Error::Code(err.code))
                                                       : ILIAS_NAMESPACE::Error(JsonRpcError(err.code)));
            }
            if constexpr (std::is_void_v<typename std::decay_t<T>::RawReturnType>) {
                return {};
            } else {
                return response.result.value();
            }
        } else {
            NEKO_LOG_ERROR("jsonrpc", "Error: response parser error");
            return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ParseError);
        }
    }

private:
    ProtocolT mProtocol;
    std::unique_ptr<DatagramClientBase> mClient = nullptr;
    uint64_t mId                                = 0;
    std::vector<char> mBuffer;
    ILIAS_NAMESPACE::Mutex mMutex;
};
NEKO_END_NAMESPACE
