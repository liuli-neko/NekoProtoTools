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
#include <ilias/sync/oneshot.hpp>
#include <ilias/task/group.hpp>
#include <ilias/task/when_all.hpp>
#include <memory>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility> // std::index_sequence, std::make_index_sequence

#include "ilias/defines.hpp"
#include "ilias/io/error.hpp"
#include "ilias/task/task.hpp"
#include "nekoproto/global/reflect.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/jsonrpc/jsonrpc_traits.hpp"
#include "nekoproto/jsonrpc/message_stream_wrapper.hpp"
#include "nekoproto/serialization/reflection.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

template <typename T>
using IoTask = ILIAS_NAMESPACE::IoTask<T>;

template <typename T, class enable = void>
class RpcMethodTraits; // 主模板前向声明

// 统一的特化，处理所有可调用类型
template <typename Callable>
class RpcMethodTraits<Callable,
                      std::void_t<typename traits::function_traits<std::remove_cvref_t<Callable>>::return_type>>
    : public traits::RpcMethodTraitsUnpacker<std::remove_cvref_t<Callable>> {
private:
    using Traits   = traits::function_traits<std::remove_cvref_t<Callable>>;
    using ArgTuple = typename Traits::arg_tuple;
    using Base     = traits::RpcMethodTraitsUnpacker<std::remove_cvref_t<Callable>>;

    using Aliases = typename traits::RpcMethodAliases<Traits, ArgTuple>::type;

public:
    // 从 Aliases 结构体中导出所有类型和常量
    using ParamsTupleType = typename Aliases::ParamsTupleType;
    using RawParamsType   = typename Aliases::RawParamsType;
    using FunctionType    = typename Aliases::FunctionType;
    using ReturnType = std::optional<std::conditional_t<std::is_void_v<typename Base::RawReturnType>, std::nullptr_t,
                                                        std::remove_cvref_t<typename Base::RawReturnType>>>;

    constexpr static int NumParams                 = Aliases::NumParams;
    constexpr static int ParamsSize                = Aliases::ParamsSize;
    constexpr static bool IsAutomaticExpansionAble = Aliases::IsAutomaticExpansionAble;
    constexpr static bool IsTopTuple               = Aliases::IsTopTuple;
    constexpr static bool IsNullAble               = Aliases::IsNullAble;
};

template <typename T>
concept RpcMethodT = requires() { std::is_constructible_v<typename RpcMethodTraits<T>::FunctionType, T>; };

template <auto Ptr>
concept RpcMethodFuncT = requires() { typename RpcMethodTraits<decltype(Ptr)>::FunctionType; };

// MARK: JsonRpc Proto
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
using JsonRpcIdType = std::variant<std::monostate, uint64_t, std::string>;

// Declaration - implementation moved to src/jsonrpc.cpp
NEKO_PROTO_API std::string to_string(JsonRpcIdType id);

template <typename T, ConstexprString... ArgNames>
struct JsonRpcRequest2;
template <typename... Args, ConstexprString... ArgNames>
struct JsonRpcRequest2<RpcMethodTraits<Args...>, ArgNames...> {
    using ParamsTupleType = typename RpcMethodTraits<Args...>::ParamsTupleType;
    // if ParamsTupleType is tuple, argnames > 0 and argnames == number of params
    // if ParamsTupleType is not tuple, argnames must == 0
    static_assert(sizeof...(ArgNames) == 0 || RpcMethodTraits<Args...>::NumParams == sizeof...(ArgNames),
                  "JsonRpcRequest2: The number of parameters and names do not match.");
    static_assert((is_std_tuple_v<ParamsTupleType> ? true : (sizeof...(ArgNames) == 0)),
                  "JsonRpcRequest2: ParamsTupleType must be tuple if argnames > 0 and not tuple if argnames == 0.");

    std::optional<std::string> jsonrpc = "2.0";
    std::string method;
    ParamsTupleType params;
    JsonRpcIdType id;

    template <typename SerializerT>
    bool save(SerializerT& serializer) const NEKO_NOEXCEPT {
        serializer.startObject(-1);
        if constexpr (sizeof...(ArgNames) == 0) {
            bool ret = serializer(NEKO_PROTO_NAME_VALUE_PAIR(jsonrpc), NEKO_PROTO_NAME_VALUE_PAIR(method),
                                  NEKO_PROTO_NAME_VALUE_PAIR(id), NEKO_PROTO_NAME_VALUE_PAIR(params));
            return serializer.endObject() && ret;
        } else {
            traits::SerializerHelperObject<const ParamsTupleType, ArgNames...> mParamsHelper(params);
            auto ret = serializer(NEKO_PROTO_NAME_VALUE_PAIR(jsonrpc), NEKO_PROTO_NAME_VALUE_PAIR(method),
                                  NEKO_PROTO_NAME_VALUE_PAIR(id));
            if constexpr (RpcMethodTraits<Args...>::NumParams > 0 && RpcMethodTraits<Args...>::ParamsSize > 0) {
                serializer(make_name_value_pair("params", mParamsHelper));
            }
            return serializer.endObject() && ret;
        }
    }

    template <typename SerializerT>
    bool load(SerializerT& serializer) NEKO_NOEXCEPT {
        serializer.startNode();
        if constexpr (sizeof...(ArgNames) == 0) {
            bool ret = serializer(NEKO_PROTO_NAME_VALUE_PAIR(jsonrpc), NEKO_PROTO_NAME_VALUE_PAIR(method),
                                  NEKO_PROTO_NAME_VALUE_PAIR(id));
            // params maybe is all is std::optional, then server will not send params
            ret = (serializer(NEKO_PROTO_NAME_VALUE_PAIR(params)) || RpcMethodTraits<Args...>::IsNullAble)&& ret;
            return serializer.finishNode() && ret;
        } else {
            traits::SerializerHelperObject<ParamsTupleType, ArgNames...> mParamsHelper(params);
            bool ret = serializer(NEKO_PROTO_NAME_VALUE_PAIR(jsonrpc), NEKO_PROTO_NAME_VALUE_PAIR(method),
                                  NEKO_PROTO_NAME_VALUE_PAIR(id));
            // params maybe is all is std::optional, then server will not send params
            ret = (serializer(make_name_value_pair("params", mParamsHelper)) || RpcMethodTraits<Args...>::IsNullAble) && ret;
            return serializer.finishNode() && ret;
        }
    }
    NEKO_SERIALIZER(jsonrpc, method, params, id)
};

struct JsonRpcRequestMethod {
    std::optional<std::string> jsonrpc;
    std::string method;
    JsonRpcIdType id;
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
// MARK: Rpc Method MetaObject
template <RpcMethodT T, ConstexprString... ArgNames>
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

    RpcMethodDynamic(std::string_view name, bool isNotification = false) noexcept {
        mName                 = std::string(name);
        this->mIsNotification = isNotification;
        description =
            std::string(traits::TypeName<RawReturnType>::name()) + " " + std::string(name) + "(" +
            std::string(traits::parameter_to_string<RawParamsType>(
                std::make_index_sequence<std::tuple_size_v<RawParamsType>>{}, traits::ArgNamesHelper<ArgNames...>{})) +
            ")";
    }

    RpcMethodDynamic(std::string_view name, FunctionType func, bool isNotification = false) noexcept
        : RpcMethodDynamic(name, isNotification) {
        this->mCoFunction = [func = std::move(func)](auto... args) mutable -> ILIAS_NAMESPACE::IoTask<RawReturnType> {
            if constexpr (std::is_void_v<RawReturnType>) {
                func(args...);
                co_return {};
            } else {
                co_return func(args...);
            }
        };
    }

    RpcMethodDynamic(std::string_view name, CoroutinesFuncType func, bool isNotification = false) noexcept
        : RpcMethodDynamic(name, isNotification) {
        this->mCoFunction = std::move(func);
    }

    RequestType request(const JsonRpcIdType& id, ParamsTupleType&& params = {}) noexcept {
        return RequestType{.method = mName.c_str(), .params = std::forward<ParamsTupleType>(params), .id = id};
    }

    static ResponseType response(const JsonRpcIdType& id, const MethodTraits::ReturnType& result) noexcept {
        return ResponseType{.result = result, .error = {}, .id = id};
    }

    static ResponseType response(const JsonRpcIdType& id, const JsonRpcErrorResponse& error) noexcept {
        return ResponseType{.result = {}, .error = error, .id = id};
    }
    template <typename U>
        requires std::is_convertible_v<U, FunctionType> && (!std::is_convertible_v<U, CoroutinesFuncType>)
    RpcMethodDynamic& operator=(U&& func) noexcept {
        if constexpr (std::is_bind_expression_v<U>) {
            return set(std::move(func));
        } else {
            return set(FunctionType(std::move(func)));
        }
    }
    template <typename U>
        requires std::is_convertible_v<U, CoroutinesFuncType>
    RpcMethodDynamic& operator=(U&& func) noexcept {
        if constexpr (std::is_bind_expression_v<U>) {
            return set(std::move(func));
        } else {
            return set(CoroutinesFuncType(std::move(func)));
        }
    }

    RpcMethodDynamic& set(FunctionType&& func) noexcept {
        this->mCoFunction = [func = std::move(func)](auto... args) mutable -> ILIAS_NAMESPACE::IoTask<RawReturnType> {
            if constexpr (std::is_void_v<RawReturnType>) {
                func(args...);
                co_return {};
            } else {
                co_return func(args...);
            }
        };
        return *this;
    }

    RpcMethodDynamic& set(CoroutinesFuncType&& func) noexcept {
        this->mCoFunction = std::move(func);
        return *this;
    }
    operator bool() const noexcept { return this->mCoFunction != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return this->mCoFunction == nullptr; }
    void clear() noexcept { this->mCoFunction = nullptr; }

    const std::string& name() const noexcept { return mName; }
    std::string_view name() noexcept { return mName; }

    std::string description;

private:
    std::string mName;
};

template <RpcMethodT T, ConstexprString MethodName, ConstexprString... ArgNames>
class RpcMethod : public RpcMethodDynamic<T, ArgNames...> {
public:
    RpcMethod() : RpcMethodDynamic<T, ArgNames...>(MethodName.view()) {}
    RpcMethod(bool isNotification) : RpcMethodDynamic<T, ArgNames...>(MethodName.view(), isNotification) {}
    using RpcMethodDynamic<T, ArgNames...>::operator=;
    operator bool() const noexcept { return this->mCoFunction != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return this->mCoFunction == nullptr; }
};

template <auto Ptr, ConstexprString... ArgNames>
class RpcMethodF : public RpcMethodDynamic<decltype(Ptr), ArgNames...> {
public:
    constexpr static std::string_view MethodName = detail::func_nameof<Ptr>;
    RpcMethodF() : RpcMethodDynamic<decltype(Ptr), ArgNames...>(MethodName) { this->operator=(Ptr); }
    RpcMethodF(bool isNotification)
        : RpcMethodDynamic<decltype(Ptr), ArgNames...>(Ptr->name(), nullptr, isNotification) {
        this->operator=(Ptr);
    }
    using RpcMethodDynamic<decltype(Ptr), ArgNames...>::operator=;
};

template <auto Ptr, typename T, ConstexprString... ArgNames>
class RpcMethodFN : public RpcMethodDynamic<decltype(Ptr), ArgNames...> {
    constexpr static std::string_view seq = ".";

public:
    constexpr static std::string_view MethodName = detail::join<detail::class_nameof<T>, seq, detail::func_nameof<Ptr>>;
    RpcMethodFN() : RpcMethodDynamic<decltype(Ptr), ArgNames...>(MethodName) { this->operator=(Ptr); }
    RpcMethodFN(bool isNotification)
        : RpcMethodDynamic<decltype(Ptr), ArgNames...>(Ptr->name(), nullptr, isNotification) {
        this->operator=(Ptr);
    }
    using RpcMethodDynamic<decltype(Ptr), ArgNames...>::operator=;
};

struct RpcMethodErrorHelper {
    using ResponseType = JsonRpcResponse<RpcMethodTraits<void(void)>>;
    static ResponseType response(const JsonRpcIdType& id, const JsonRpcErrorResponse& error) noexcept {
        return ResponseType{.result = {}, .error = error, .id = id};
    }
};

class JsonRpcServerImp;
class RpcMethodWrapper {
public:
    RpcMethodWrapper()                                                                       = default;
    RpcMethodWrapper(RpcMethodWrapper&&)                                                     = default;
    virtual ~RpcMethodWrapper()                                                              = default;
    virtual auto call(JsonSerializer::InputSerializer& in, JsonSerializer::OutputSerializer& out,
                      JsonRpcRequestMethod&& method) noexcept -> ILIAS_NAMESPACE::Task<void> = 0;
    auto operator()(JsonSerializer::InputSerializer& in, JsonSerializer::OutputSerializer& out,
                    JsonRpcRequestMethod&& method) noexcept {
        return call(in, out, std::move(method));
    }
    virtual auto name() noexcept -> std::string_view        = 0;
    virtual auto description() noexcept -> std::string_view = 0;
    virtual auto isBind() noexcept -> bool                  = 0;
};

template <typename T>
struct RpcMethodTypeHelper {
    using MethodType = T;
};

template <typename T>
struct RpcMethodTypeHelper<T*> {
    using MethodType = T;
};

template <typename T>
struct RpcMethodTypeHelper<std::unique_ptr<T>> {
    using MethodType = T;
};

template <typename T>
class RpcMethodWrapperImpl : public RpcMethodWrapper {
public:
    using MethodType = typename RpcMethodTypeHelper<T>::MethodType;
    RpcMethodWrapperImpl(T methodData, JsonRpcServerImp* self) : mMethodData(std::move(methodData)), mSelf(self) {}
    auto call(JsonSerializer::InputSerializer& in, JsonSerializer::OutputSerializer& out,
              JsonRpcRequestMethod&& method) noexcept -> ILIAS_NAMESPACE::Task<void> override;
    auto name() noexcept -> std::string_view override { return mMethodData->name(); }
    auto description() noexcept -> std::string_view override { return mMethodData->description; }
    auto isBind() noexcept -> bool override { return (bool)(*mMethodData); }

private:
    T mMethodData;
    JsonRpcServerImp* mSelf = nullptr;
};
// MARK: jsonrpc server
class NEKO_PROTO_API JsonRpcServerImp {
public:
    struct MethodData {
        std::string_view name;
        std::string_view description;
        bool isBind = false;
    };

private:
    auto _makeBatch(JsonSerializer::InputSerializer& in, JsonSerializer::OutputSerializer& out, int batchSize) noexcept
        -> void;

public:
    JsonRpcServerImp([[maybe_unused]] ILIAS_NAMESPACE::IoContext& ctx) : mTaskScope() {}
    ~JsonRpcServerImp() {
        cancelAll();
        mTaskScope.waitAll().wait();
    }
    template <typename T>
    auto registerRpcMethod(T& metadata) noexcept -> void {
        if (auto item = mHandlers.find(metadata.name()); item != mHandlers.end()) {
            NEKO_LOG_ERROR("jsonrpc", "Method {} exist!!!", metadata.name());
            return;
        }
        mHandlers[metadata.name()] = std::make_unique<RpcMethodWrapperImpl<T*>>(&metadata, this);
    }
    auto methodDatas() noexcept -> std::vector<MethodData>;
    auto methodDatas(std::string_view name) noexcept -> MethodData;
    template <typename RetT, typename... Args, ConstexprString... ArgNames>
    auto bindRpcMethod(std::string_view name, traits::FunctionT<RetT(Args...)> func,
                       traits::ArgNamesHelper<ArgNames...> /*unused*/ = {}) noexcept -> void {
        return _registerRpcMethod<RpcMethodDynamic<RetT(Args...), ArgNames...>>(name, std::move(func));
    }
    template <typename RetT, typename... Args, ConstexprString... ArgNames>
    auto bindRpcMethod(std::string_view name, traits::FunctionT<ILIAS_NAMESPACE::IoTask<RetT>(Args...)> func,
                       traits::ArgNamesHelper<ArgNames...> /*unused*/ = {}) noexcept -> void {
        _registerRpcMethod<RpcMethodDynamic<RetT(Args...), ArgNames...>>(name, std::move(func));
    }

    auto processRequest(const char* data, std::size_t size, detail::IMessageStream* client) noexcept
        -> ILIAS_NAMESPACE::Task<std::vector<char>>;
    void cancel(JsonRpcIdType id) noexcept;
    void cancelAll();
    auto receiveLoop(detail::IMessageStream* client) noexcept -> ILIAS_NAMESPACE::Task<void>;
    auto getCurrentIds() const noexcept -> const std::vector<JsonRpcIdType>&;

private:
    template <typename T>
    auto _handle(bool success, auto request, auto& out, auto method, T& metadata) noexcept
        -> ILIAS_NAMESPACE::Task<void> {
        if (!success) {
            NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc request");
            co_return _handleError<T>(out, std::move(method), (int64_t)JsonRpcError::InvalidRequest,
                                      JsonRpcErrorCategory::instance().message((int64_t)JsonRpcError::InvalidRequest));
        }
        auto handle =
            mTaskScope.spawn([&, this, request = std::move(request), method = method]() -> ILIAS_NAMESPACE::Task<void> {
                ILIAS_NAMESPACE::Result<typename T::RawReturnType, std::error_code> result =
                    ILIAS_NAMESPACE::Unexpected(ILIAS_NAMESPACE::SystemError::Canceled);
                auto onfinally = [&]() -> ILIAS_NAMESPACE::Task<void> {
                    co_return _processMethodReturn<T>(metadata, std::move(result), out, std::move(method));
                };
                co_await ([&result, request = std::move(request)](T& metadata) -> ILIAS_NAMESPACE::Task<void> {
                    if constexpr (T::NumParams == 0) {
                        result = co_await metadata();
                    } else if constexpr (T::IsAutomaticExpansionAble || T::IsTopTuple) {
                        result = co_await metadata(std::move(request.params));
                    } else {
                        result = co_await std::apply(metadata, std::move(request.params));
                    }
                    co_return;
                }(metadata) | ILIAS_NAMESPACE::finally(onfinally));
                co_return;
            });
        if (!std::holds_alternative<std::monostate>(method.id)) {
            mCancelHandles[method.id] = std::move(handle);
        }
    }
    // RpcMethodDynamic<RetT(Args...), ArgNames...>
    template <typename T, typename Callable>
    auto _registerRpcMethod(std::string_view name, traits::FunctionT<Callable> func) noexcept -> void {
        mHandlers[name] = std::make_unique<RpcMethodWrapperImpl<std::unique_ptr<T>>>(
            std::make_unique<T>(name, std::move(func)), this);
    }

    template <typename T, typename RetT>
    auto _handleSuccess(T& methodData, JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod method,
                        RetT&& ret) noexcept -> void {
        if (std::holds_alternative<std::monostate>(method.id)) { // notification
            return;
        }
        if (!method.jsonrpc.has_value() && method.id.index() == 1) { // notification in jsonrpc 1.0
            return;
        }
        if constexpr (std::is_void_v<typename T::RawReturnType>) {
            if (!out(methodData.response(method.id, nullptr))) {
                NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
                return;
            }
        } else {
            if (!out(methodData.response(method.id, std::forward<RetT>(ret)))) {
                NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
                return;
            }
        }
    }

    template <typename T>
    auto _handleError(JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod method, int64_t code,
                      std::string message) noexcept -> void {
        if (std::holds_alternative<std::monostate>(method.id)) { // notification
            return;
        }
        if (!method.jsonrpc.has_value() && method.id.index() == 1) { // notification in jsonrpc 1.0
            return;
        }
        if (!out(T::response(method.id, {code, message}))) {
            NEKO_LOG_ERROR("jsonrpc", "invalid jsonrpc response");
            return;
        }
    }

    template <typename T>
    auto _processMethodReturn(T& methodData, ILIAS_NAMESPACE::Result<typename T::RawReturnType, std::error_code> result,
                              JsonSerializer::OutputSerializer& out, JsonRpcRequestMethod method) noexcept -> void {
        if constexpr (std::is_void_v<typename T::RawReturnType>) {
            if (result) {
                return _handleSuccess<T, std::nullptr_t>(methodData, out, std::move(method), nullptr);
            }
            NEKO_LOG_ERROR("jsonrpc", "method {} failed to execute, {}", method.method, result.error().message());
            return _handleError<T>(out, std::move(method), result.error().value(), result.error().message());
        } else {
            if (result) {
                return _handleSuccess<T, typename T::RawReturnType>(methodData, out, std::move(method),
                                                                    std::move(result.value()));
            }
            NEKO_LOG_WARN("jsonrpc", "method {} failed to execute, {}", method.method, result.error().message());
            return _handleError<T>(out, std::move(method), result.error().value(), result.error().message());
        }
    }

private:
    std::vector<detail::JsonRpcIdType> mCurrentIds;
    std::map<JsonRpcIdType, ILIAS_NAMESPACE::StopHandle> mCancelHandles;
    ILIAS_NAMESPACE::TaskGroup<void> mTaskScope;
    std::map<std::string_view, std::unique_ptr<RpcMethodWrapper>> mHandlers;

    template <typename T>
    friend class RpcMethodWrapperImpl;
};

template <typename T>
auto RpcMethodWrapperImpl<T>::call(JsonSerializer::InputSerializer& in, JsonSerializer::OutputSerializer& out,
                                   JsonRpcRequestMethod&& method) noexcept -> ILIAS_NAMESPACE::Task<void> {
    typename MethodType::RequestType request;
    auto success = in(request);
    return mSelf->_handle(success, request, out, std::move(method), *mMethodData);
};

} // namespace detail
using detail::RpcMethod;
using detail::RpcMethodF;

template <typename ProtocolT>
class JsonRpcServer {
private:
    using MethodData = detail::JsonRpcServerImp::MethodData;
    struct {
        RpcMethod<std::vector<std::string>(), "rpc.get_method_list"> getMethodList;
        RpcMethod<std::vector<std::string>(), "rpc.get_method_info_list"> getMethodInfoList;
        RpcMethod<std::string(std::string), "rpc.get_method_info", "method_name"> getMethodInfo;
        RpcMethod<std::vector<std::string>(), "rpc.get_bind_method_list"> getBindedMethodList;
    } mRpc;

public:
    JsonRpcServer(ILIAS_NAMESPACE::IoContext& ctx) : mImp(ctx), mScop() { _init(); }
    ~JsonRpcServer() {
        close();
        wait().wait();
    }
    auto operator->() noexcept { return &mProtocol; }
    auto operator->() const noexcept { return &mProtocol; }
    auto close() noexcept -> void {
        NEKO_LOG_INFO("jsonrpc", "close");
        for (auto& client : mTransports) {
            client->cancel();
        }
        if (mServer != nullptr) {
            mServer->cancel();
        }
        mScop.stop();
        for (auto& client : mTransports) {
            client->close();
        }
        if (mServer != nullptr) {
            mServer->close();
        }
    }

    auto wait() -> ILIAS_NAMESPACE::Task<void> { co_await mScop.waitAll(); }
    auto cancel(uint64_t id) noexcept -> void { mImp.cancel(id); }
    auto cancel(const std::string& id) noexcept -> void { mImp.cancel(id); }
    auto cancelAll() noexcept -> void { mImp.cancelAll(); }
    auto getCurrentIds() const noexcept -> const std::vector<detail::JsonRpcIdType>& { return mImp.getCurrentIds(); }

    template <typename ClientT>
        requires MessageStreamClient<ClientT> || std::is_same_v<ClientT, std::unique_ptr<detail::IMessageStream>>
    auto addTransport(ClientT client) noexcept -> void {
        auto item = mTransports.begin();
        if constexpr (std::is_same_v<ClientT, std::unique_ptr<detail::IMessageStream>>) {
            item = mTransports.emplace(mTransports.end(), std::move(client));
        } else {
            item = mTransports.emplace(mTransports.end(),
                                       std::make_unique<detail::MessageStreamWrapper<ClientT>>(std::move(client)));
        }
        mScop.spawn([this, item]() -> ILIAS_NAMESPACE::Task<void> {
            NEKO_LOG_INFO("jsonrpc", "Starting receive loop({}) for transport", ((void*)item->get()));
            co_await mImp.receiveLoop((*item).get());
            NEKO_LOG_INFO("jsonrpc", "Stoped receive loop({}) for transport", ((void*)item->get()));
            mTransports.erase(item);
        });
    }

    template <typename ListenerT>
        requires MessageStreamListener<ListenerT> ||
                 std::is_same_v<ListenerT, std::unique_ptr<detail::IMessageListener>>
    auto setListener(ListenerT listener) noexcept -> void {
        if constexpr (std::is_same_v<ListenerT, std::unique_ptr<detail::IMessageListener>>) {
            mServer = std::move(listener);
        } else {
            mServer = std::make_unique<detail::MessageListenerWrapper<ListenerT>>(std::move(listener));
        }
        mScop.spawn(_acceptLoop());
    }

    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, traits::FunctionT<RetT(Args...)> func) noexcept -> void {
        static_assert(sizeof...(ArgNames) == 0 || sizeof...(ArgNames) == sizeof...(Args),
                      "bindMethod: The number of parameters and names do not match.");
        mImp.bindRpcMethod(name, std::move(func), traits::ArgNamesHelper<ArgNames...>{});
    }

    template <auto Ptr, ConstexprString... ArgNames>
        requires detail::RpcMethodFuncT<Ptr>
    auto bindMethod() noexcept -> void {
        mImp.bindRpcMethod(detail::RpcMethodF<Ptr, ArgNames...>::MethodName, std::function(Ptr),
                           traits::ArgNamesHelper<ArgNames...>{});
    }

    template <ConstexprString... ArgNames, typename RetT, typename... Args>
    auto bindMethod(std::string_view name, traits::FunctionT<ILIAS_NAMESPACE::IoTask<RetT>(Args...)> func) noexcept
        -> void {
        mImp.bindRpcMethod(name, std::move(func), traits::ArgNamesHelper<ArgNames...>{});
    }

    auto callMethod(std::string_view json) noexcept -> ILIAS_NAMESPACE::Task<std::vector<char>> {
        co_return co_await mImp.processRequest(json.data(), json.size(), nullptr);
    }

    auto methodDatas() noexcept -> std::vector<MethodData> { return mImp.methodDatas(); }

    auto methodDatas(std::string_view name) noexcept -> MethodData { return mImp.methodDatas(name); }

private:
    auto _acceptLoop() noexcept -> ILIAS_NAMESPACE::Task<void> {
        while (mServer != nullptr) {
            if (auto ret = co_await mServer->accept(); ret) {
                if (mTransports.size() > 0 && mTransports.back() == ret.value()) {
                    break;
                }
                addTransport(std::move(ret.value()));
            } else {
                if (ret.error() != ILIAS_NAMESPACE::IoError::Canceled) {
                    NEKO_LOG_WARN("jsonrpc", "accepting exit with: {}", ret.error().message());
                }
                break;
            }
        }
    }

    auto _init() noexcept {
        auto registerRpcMethod = [this]<typename T>(T& rpcMethodData) { mImp.registerRpcMethod(rpcMethodData); };
        Reflect<ProtocolT>::forEach(mProtocol, registerRpcMethod);
        Reflect<decltype(mRpc)>::forEach(mRpc, registerRpcMethod);
        mRpc.getMethodInfo = [this](std::string methodName) -> ilias::IoTask<std::string> {
            if (auto ret = mImp.methodDatas(methodName); !ret.name.empty()) {
                co_return std::string(ret.description);
            }
            co_return std::string("Method not found!");
        };
        mRpc.getMethodInfoList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> methodDesList;
            for (auto& item : methodDatas()) {
                methodDesList.emplace_back(item.description);
            }
            co_return methodDesList;
        };
        mRpc.getMethodList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> methodList;
            for (auto& item : methodDatas()) {
                methodList.emplace_back(item.name);
            }
            co_return methodList;
        };
        mRpc.getBindedMethodList = [this]() -> ilias::IoTask<std::vector<std::string>> {
            std::vector<std::string> methodList;
            for (auto& item : methodDatas()) {
                if (item.isBind) {
                    methodList.emplace_back(item.name);
                }
            }
            co_return methodList;
        };
    }

private:
    ProtocolT mProtocol;
    detail::JsonRpcServerImp mImp;
    std::unique_ptr<detail::IMessageListener> mServer;
    std::list<std::unique_ptr<detail::IMessageStream>> mTransports;
    std::map<std::string_view, std::unique_ptr<MethodData>> mMethodDatas;
    ILIAS_NAMESPACE::TaskGroup<void> mScop;
};
// MARK: jsonrpc client
template <typename ProtocolT>
class JsonRpcClient {
public:
    JsonRpcClient(ILIAS_NAMESPACE::IoContext& /*unused*/) {
        Reflect<ProtocolT>::forEach(mProtocol,
                                    [this](auto& rpcMethodData) { this->_registerRpcMethod(rpcMethodData); });
    }
    ~JsonRpcClient() { close(); }
    auto operator->() const noexcept { return &mProtocol; }
    auto close() noexcept -> void {
        if (mTransport != nullptr) {
            mTransport->cancel();
        }
        if (mTransport != nullptr) {
            mTransport->close();
        }
    }

    auto isConnected() const noexcept -> bool { return mTransport != nullptr; }

    template <typename T>
        requires MessageStreamClient<T> || std::is_same_v<T, std::unique_ptr<detail::IMessageStream>>
    auto setTransport(T client) noexcept -> void {
        if constexpr (std::is_same_v<T, std::unique_ptr<detail::IMessageStream>>) {
            mTransport = std::move(client);
        } else {
            mTransport = std::make_unique<detail::MessageStreamWrapper<T>>(std::move(client));
        }
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto callRemote(std::string_view name, Args... args) noexcept -> ILIAS_NAMESPACE::IoTask<RetT> {
        using CoroutinesFuncType = typename detail::RpcMethodDynamic<RetT(Args...), ArgNames...>::CoroutinesFuncType;
        detail::RpcMethodDynamic<RetT(Args...), ArgNames...> metadata(name, (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto callRemote(Args... args) noexcept
        -> ILIAS_NAMESPACE::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using CoroutinesFuncType = typename detail::RpcMethodDynamic<decltype(Ptr), ArgNames...>::CoroutinesFuncType;
        detail::RpcMethodDynamic<decltype(Ptr), ArgNames...> metadata(RpcMethodF<Ptr>::MethodName,
                                                                      (CoroutinesFuncType)(nullptr), false);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <typename RetT, ConstexprString... ArgNames, typename... Args>
    auto notifyRemote(std::string_view name, Args... args) noexcept -> ILIAS_NAMESPACE::IoTask<RetT> {
        using CoroutinesFuncType = typename detail::RpcMethodDynamic<RetT(Args...), ArgNames...>::CoroutinesFuncType;
        detail::RpcMethodDynamic<RetT(Args...), ArgNames...> metadata(name, (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

    template <auto Ptr, ConstexprString... ArgNames, typename... Args>
        requires detail::RpcMethodFuncT<Ptr>
    auto notifyRemote(Args... args) noexcept
        -> ILIAS_NAMESPACE::IoTask<typename traits::function_traits<decltype(Ptr)>::return_type> {
        using CoroutinesFuncType = typename detail::RpcMethodDynamic<decltype(Ptr), ArgNames...>::CoroutinesFuncType;
        detail::RpcMethodDynamic<decltype(Ptr), ArgNames...> metadata(RpcMethodF<Ptr>::MethodName,
                                                                      (CoroutinesFuncType)(nullptr), true);
        co_return co_await _callRemote(metadata, std::forward<Args>(args)...);
    }

private:
    template <typename T>
    void _registerRpcMethod(T&& metadata) noexcept {
        metadata = (typename std::decay_t<T>::CoroutinesFuncType)[this, &metadata](auto... args) noexcept
            -> ILIAS_NAMESPACE::IoTask<typename std::decay_t<T>::RawReturnType> {
            return _callRemote<T, decltype(args)...>(metadata, args...);
        };
    }

    template <typename T, typename... Args>
    auto _callRemote(T& metadata, Args... args) noexcept
        -> ILIAS_NAMESPACE::IoTask<typename std::decay_t<T>::RawReturnType> {
        if (mTransport == nullptr) {
            co_return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ClientNotInit);
        }
        auto grid = co_await mMutex.lock();
        typename std::decay_t<T>::RequestType request;
        if (auto ret =
                _sendRequest<T, Args...>(metadata, metadata.isNotification(), request, std::forward<Args>(args)...);
            ret) {
            if (auto ret1 = co_await mTransport->send({reinterpret_cast<std::byte*>(mBuffer.data()), mBuffer.size()});
                !ret1) {
                NEKO_LOG_ERROR("jsonrpc", "send {} request failed", request.method);
                co_return ILIAS_NAMESPACE::Unexpected(ret1.error());
            }
        } else {
            co_return ILIAS_NAMESPACE::Unexpected(ret.error());
        }
        if (!metadata.isNotification()) {
            typename std::decay_t<T>::ResponseType respone;
            std::vector<std::byte> buffer;
            if (auto ret = co_await mTransport->recv(buffer); ret) {
                if (auto ret1 = _recvResponse<T>(buffer, respone, request.id); !ret1) {
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
        co_return {};
    }

    template <typename T, typename... Args>
    auto _sendRequest(T& methodData, bool notification, typename std::decay_t<T>::RequestType& request,
                      Args... args) noexcept -> ILIAS_NAMESPACE::Result<void, std::error_code> {
        if constexpr (traits::optional_like_type<typename std::decay_t<T>::ParamsTupleType>::value) {
            if (notification) {
                request    = methodData.request(std::monostate{});
                request.id = std::monostate{};
            } else {
                request = methodData.request(mId++);
            }
        } else {
            if constexpr (std::decay_t<T>::IsAutomaticExpansionAble || std::decay_t<T>::IsTopTuple) {
                if (notification) {
                    request    = methodData.request(std::monostate{}, std::forward<Args>(args)...);
                    request.id = std::monostate{};
                } else {
                    request = methodData.request(mId++, std::forward<Args>(args)...);
                }
            } else {
                if (notification) {
                    request    = methodData.request(std::monostate{}, std::forward_as_tuple(args...));
                    request.id = std::monostate{};
                } else {
                    request = methodData.request(mId++, std::forward_as_tuple(args...));
                }
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
                       detail::JsonRpcIdType& id) noexcept
        -> ILIAS_NAMESPACE::Result<typename std::decay_t<T>::RawReturnType, std::error_code> {
        NEKO_LOG_INFO("jsonrpc", "recv response: {}",
                      std::string_view{reinterpret_cast<const char*>(buffer.data()), buffer.size()});
        if (auto in = JsonSerializer::InputSerializer(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            in(response)) {
            if (response.id != id) {
                NEKO_LOG_ERROR("jsonrpc", "id mismatch: {} != {}", detail::to_string(response.id),
                               detail::to_string(id));
                return ILIAS_NAMESPACE::Unexpected(JsonRpcError::ResponseIdNotMatch);
            }
            if (response.error.has_value()) {
                detail::JsonRpcErrorResponse err = response.error.value();
                NEKO_LOG_WARN("jsonrpc", "Error({}): {}", err.code, err.message);
                return ILIAS_NAMESPACE::Unexpected(err.code > 0
                                                       ? std::error_code(ILIAS_NAMESPACE::IoError::Code(err.code))
                                                       : std::error_code(JsonRpcError(err.code)));
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
    std::unique_ptr<detail::IMessageStream> mTransport = nullptr;
    uint64_t mId                                       = 0;
    std::vector<char> mBuffer;
    ILIAS_NAMESPACE::Mutex mMutex;
};
NEKO_END_NAMESPACE
