#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <ilias/io/error.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/result.hpp>

#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/jsonrpc/protocol.hpp"
#include "nekoproto/rpc/client.hpp"
#include "nekoproto/rpc/concepts.hpp"
#include "nekoproto/rpc/error.hpp"
#include "nekoproto/rpc/server.hpp"
#include "nekoproto/serialization/json_serializer.hpp"

NEKO_BEGIN_NAMESPACE

struct JsonRpcBackend {
    using Id             = detail::JsonRpcIdType;
    using Message        = std::vector<char>;
    using ResponseValues = detail::JsonRpcResponseValues;

    struct DecodedRequest {
        JsonSerializer::JsonValue value;
        detail::JsonRpcRequestMethod method;
    };

    struct DecodeResult {
        bool ok    = false;
        bool batch = false;
        std::vector<DecodedRequest> requests;
        ResponseValues responses;
    };

    struct EncodedRequest {
        Message message;
        Id id;
    };

    struct Options {
        std::size_t max_message_bytes = 16U * 1024U * 1024U;
        std::size_t max_pending_calls = 1024U;
        std::size_t max_inflight_requests_per_connection = 1024U;
        std::size_t max_active_requests_global = 4096U;
        std::size_t max_queued_requests_global = 4096U;
        std::optional<std::chrono::nanoseconds> request_timeout;
    };

    struct ClientContext {
        Options options;
        std::uint64_t next_id = 0;
    };

    struct ServerContext {
        Options options;
    };

    struct PeerSession {};

public:
    template <typename T>
    static consteval bool serializable() {
        if constexpr (std::is_void_v<T>) {
            return true;
        } else {
            return traits::Serializable<T>;
        }
    }

    static auto makeClientContext(Options options) -> ClientContext {
        return ClientContext{.options = std::move(options), .next_id = 0};
    }

    static auto makeClientContext() -> ClientContext { return makeClientContext(Options{}); }

    template <typename Methods>
    static auto makeServerContext(Options options, Methods&& /*methods*/) -> ServerContext {
        return ServerContext{.options = std::move(options)};
    }

    template <typename Methods>
    static auto makeServerContext(Methods&& methods) -> ServerContext {
        return makeServerContext(Options{}, std::forward<Methods>(methods));
    }

    static auto makeClientPeerSession(const ClientContext& /*context*/) -> PeerSession { return {}; }
    static auto makeServerPeerSession(const ServerContext& /*context*/) -> PeerSession { return {}; }

    static auto validateMessage(const ClientContext& context, std::span<const std::byte> message)
        -> ilias::Result<void, std::error_code> {
        if (message.size() > context.options.max_message_bytes) {
            return ilias::Err(JsonRpcError::MessageToolLarge);
        }
        return {};
    }

    static auto validateMessage(const ServerContext& context, std::span<const std::byte> message)
        -> ilias::Result<void, std::error_code> {
        if (message.size() > context.options.max_message_bytes) {
            return ilias::Err(JsonRpcError::MessageToolLarge);
        }
        return {};
    }

    static auto refreshMethodCatalog(ServerContext& /*context*/, std::vector<detail::RpcMethodMetadata> /*methods*/)
        -> void {}

    static DecodeResult decodeIncoming(ServerContext& context, PeerSession& /*session*/,
                                       std::span<const std::byte> message) {
        DecodeResult result;
        result.ok = true;
        if (message.size() > context.options.max_message_bytes) {
            _appendProtocolError(result.responses, {}, JsonRpcError::MessageToolLarge);
            return result;
        }

        JsonSerializer::JsonValue root;
        JsonSerializer::InputSerializer rootIn(reinterpret_cast<const char*>(message.data()), message.size());
        if (!rootIn(root)) {
            _appendProtocolError(result.responses, {}, JsonRpcError::ParseError);
            return result;
        }

        result.batch = root.isArray();
        std::vector<JsonSerializer::JsonValue> rawRequests;
        if (result.batch) {
            if (root.size() == 0U) {
                result.batch = false;
                _appendProtocolError(result.responses, {}, JsonRpcError::InvalidRequest);
                return result;
            }
            rawRequests.reserve(root.size());
            for (std::size_t ix = 0; ix < root.size(); ++ix) {
                rawRequests.emplace_back(root[ix]);
            }
        } else {
            rawRequests.emplace_back(root);
        }

        for (auto& requestValue : rawRequests) {
            JsonSerializer::InputSerializer methodIn(requestValue);
            detail::JsonRpcRequestMethod method;
            if (!requestValue.isObject() || !methodIn(method) || method.jsonrpc != std::optional<std::string>{"2.0"}) {
                _appendProtocolError(result.responses, {}, JsonRpcError::InvalidRequest);
                continue;
            }
            result.requests.push_back({std::move(requestValue), std::move(method)});
        }
        return result;
    }

    static auto methodName(const DecodedRequest& request) noexcept -> std::string_view { return request.method.method; }
    static auto id(const DecodedRequest& request) noexcept -> const Id& { return request.method.id; }

    static bool expectsResponse(const DecodedRequest& request) noexcept {
        if (std::holds_alternative<std::monostate>(request.method.id)) {
            return false;
        }
        if (!request.method.jsonrpc.has_value() && request.method.id.index() == 1) {
            return false;
        }
        return true;
    }

    template <typename Method>
    static auto decodeParams(const DecodedRequest& request, const Method& method)
        -> ilias::Result<typename detail::JsonRpcMethodTraits<typename Method::MethodTraits>::ParamsTupleType,
                         std::error_code> {
        using Request = detail::JsonRpcRequest2<typename Method::MethodTraits>;
        static_assert(BackendSerializable<JsonRpcBackend, Request>,
                      "JsonRpcBackend: method parameters are not serializable by JsonSerializer");
        Request decoded;
        detail::JsonRpcMethodContext context{.argNames = method.rpcArgNames()};
        detail::JsonRpcRequestWithContext<Request> decodedWithContext{decoded, context};
        JsonSerializer::InputSerializer in(request.value);
        if (in(decodedWithContext)) {
            return std::move(decoded.params);
        }
        return ilias::Err(JsonRpcError::InvalidRequest);
    }

    template <typename Method>
    static auto invoke(Method& method,
                       typename detail::JsonRpcMethodTraits<typename Method::MethodTraits>::ParamsTupleType params)
        -> ilias::IoTask<typename Method::RawReturnType> {
        using JsonTraits = detail::JsonRpcMethodTraits<typename Method::MethodTraits>;
        if constexpr (JsonTraits::NumParams == 0) {
            if constexpr (std::is_void_v<typename Method::RawReturnType>) {
                co_await method();
                co_return {};
            } else {
                co_return co_await method();
            }
        } else if constexpr (JsonTraits::IsAutomaticExpansionAble || JsonTraits::IsTopTuple) {
            if constexpr (std::is_void_v<typename Method::RawReturnType>) {
                co_await method(std::move(params));
                co_return {};
            } else {
                co_return co_await method(std::move(params));
            }
        } else {
            if constexpr (std::is_void_v<typename Method::RawReturnType>) {
                co_await std::apply(method, std::move(params));
                co_return {};
            } else {
                co_return co_await std::apply(method, std::move(params));
            }
        }
    }

    template <typename Method>
    static void appendMethodReturn(ResponseValues& responses, const DecodedRequest& request,
                                   ilias::Result<typename Method::RawReturnType, std::error_code> result) {
        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            if (result) {
                appendSuccess<Method>(responses, request, nullptr);
            } else {
                appendError(responses, request, result.error());
            }
        } else {
            if (result) {
                appendSuccess<Method>(responses, request, std::move(result.value()));
            } else {
                appendError(responses, request, result.error());
            }
        }
    }

    template <typename Method, typename RetT>
    static void appendSuccess(ResponseValues& responses, const DecodedRequest& request, RetT&& ret) {
        if (!expectsResponse(request)) {
            return;
        }
        using Response = detail::JsonRpcResponse<typename Method::MethodTraits>;
        static_assert(BackendSerializable<JsonRpcBackend, Response>,
                      "JsonRpcBackend: method return type is not serializable by JsonSerializer");
        Response response;
        response.id = request.method.id;
        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            response.result = nullptr;
        } else {
            response.result = std::forward<RetT>(ret);
        }
        auto value = to_json_value(response);
        if (value) {
            responses.emplace_back(std::move(value.value()));
        } else {
            NEKO_LOG_ERROR("rpc", "serialize JSON-RPC success response failed: {}", value.error().msg);
            _appendProtocolError(responses, request.method.id, JsonRpcError::InternalError);
        }
    }

    static void appendError(ResponseValues& responses, const DecodedRequest& request, std::error_code error) {
        if (!expectsResponse(request)) {
            return;
        }
        _appendProtocolError(responses, request.method.id, error);
    }

    static Message encodeResponses(const ResponseValues& responses, bool batch) {
        Message buffer;
        if (responses.empty()) {
            return buffer;
        }
        JsonSerializer::OutputSerializer out(buffer);
        const bool written = batch ? out(responses) : out(responses.front());
        if (!written || !out.end()) {
            NEKO_LOG_ERROR("rpc", "serialize JSON-RPC response envelope failed");
            buffer.clear();
        }
        return buffer;
    }

    static Message encodeResponses(ServerContext& context, PeerSession& /*session*/,
                                   const ResponseValues& responses, bool batch) {
        auto buffer = encodeResponses(responses, batch);
        if (buffer.size() > context.options.max_message_bytes) {
            buffer.clear();
        }
        return buffer;
    }

    template <typename Method, typename... Args>
    static auto encodeRequest(Method& method, bool notification, std::uint64_t& nextId, Args&&... args)
        -> ilias::Result<EncodedRequest, std::error_code> {
        ClientContext context;
        context.next_id = nextId;
        PeerSession session;
        auto encoded = encodeRequest(context, session, method, notification, std::forward<Args>(args)...);
        nextId       = context.next_id;
        return encoded;
    }

    template <typename Method, typename... Args>
    static auto encodeRequest(ClientContext& context, PeerSession& /*session*/, Method& method, bool notification,
                              Args&&... args) -> ilias::Result<EncodedRequest, std::error_code> {
        using Request = detail::JsonRpcRequest2<typename Method::MethodTraits>;
        static_assert(BackendSerializable<JsonRpcBackend, Request>,
                      "JsonRpcBackend: request parameters are not serializable by JsonSerializer");
        Request request;
        request.method = std::string(method.name());
        if (notification) {
            request.id = std::monostate{};
        } else {
            request.id = context.next_id++;
        }
        fillParams<Method>(request, std::forward<Args>(args)...);

        Message buffer;
        JsonSerializer::OutputSerializer out(buffer);
        detail::JsonRpcMethodContext methodContext{.argNames = method.rpcArgNames()};
        detail::JsonRpcRequestWithContext<Request> requestWithContext{request, methodContext};
        if (out(requestWithContext) && out.end()) {
            if (buffer.size() > context.options.max_message_bytes) {
                return ilias::Err(JsonRpcError::MessageToolLarge);
            }
            return EncodedRequest{.message = std::move(buffer), .id = request.id};
        }
        return ilias::Err(JsonRpcError::InvalidRequest);
    }

    template <typename Method>
    static auto decodeResponse(std::span<const std::byte> buffer, const Id& expectedId)
        -> ilias::Result<typename Method::RawReturnType, std::error_code> {
        ClientContext context;
        PeerSession session;
        return decodeResponse<Method>(context, session, buffer, expectedId);
    }

    template <typename Method>
    static auto decodeResponse(ClientContext& context, PeerSession& /*session*/, std::span<const std::byte> buffer,
                               const Id& expectedId) -> ilias::Result<typename Method::RawReturnType, std::error_code> {
        using Response = detail::JsonRpcResponse<typename Method::MethodTraits>;
        static_assert(BackendSerializable<JsonRpcBackend, Response>,
                      "JsonRpcBackend: response type is not serializable by JsonSerializer");
        if (buffer.size() > context.options.max_message_bytes) {
            return ilias::Err(JsonRpcError::MessageToolLarge);
        }
        Response response;
        JsonSerializer::InputSerializer in(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (!in(response)) {
            return ilias::Err(JsonRpcError::ParseError);
        }
        if (response.jsonrpc != "2.0") {
            return ilias::Err(JsonRpcError::InvalidRequest);
        }
        if (response.id != expectedId) {
            return ilias::Err(JsonRpcError::ResponseIdNotMatch);
        }
        if (response.error.has_value()) {
            if (response.result.has_value()) {
                return ilias::Err(JsonRpcError::InvalidRequest);
            }
            auto err = response.error.value();
            return ilias::Err(err.code > 0 ? std::error_code(ilias::IoError::Code(err.code))
                                           : std::error_code(JsonRpcError(err.code)));
        }
        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            return {};
        } else {
            if (!response.result.has_value()) {
                return ilias::Err(JsonRpcError::InvalidRequest);
            }
            return response.result.value();
        }
    }

    static std::error_code clientNotInitError() { return JsonRpcError::ClientNotInit; }
    static std::error_code notificationOk() { return JsonRpcError::Ok; }

    template <typename Endpoint>
    static auto ensureClientReady(ClientContext& /*context*/, PeerSession& /*session*/, Endpoint& /*endpoint*/)
        -> ilias::IoTask<void> {
        co_return {};
    }

    static bool handleClientControl(ClientContext& /*context*/, PeerSession& /*session*/,
                                    std::span<const std::byte> /*message*/) {
        return false;
    }

    static auto responseId(std::span<const std::byte> message) -> std::optional<Id> {
        JsonSerializer::JsonValue root;
        JsonSerializer::InputSerializer rootIn(reinterpret_cast<const char*>(message.data()), message.size());
        if (!rootIn(root) || !root.isObject()) {
            return std::nullopt;
        }
        const auto idValue = root["id"];
        if (!idValue.hasValue()) {
            return std::nullopt;
        }
        Id id;
        JsonSerializer::InputSerializer idIn(idValue);
        if (!idIn(id)) {
            return std::nullopt;
        }
        return id;
    }

    template <typename Endpoint>
    static auto handleServerControl(ServerContext& /*context*/, PeerSession& /*session*/, Endpoint& /*endpoint*/,
                                    std::span<const std::byte> /*message*/) -> ilias::IoTask<bool> {
        co_return false;
    }

    template <ilias::Stream StreamT>
    static auto makeEndpoint(StreamT stream) {
        return detail::LengthPrefixedStreamMessageEndpoint<StreamT>{std::move(stream), JsonRpcError::InvalidRequest};
    }

    template <ilias::Stream StreamT>
    static auto makeEndpoint(StreamT stream, const Options& options) {
        return detail::LengthPrefixedStreamMessageEndpoint<StreamT>{
            std::move(stream), JsonRpcError::InvalidRequest, options.max_message_bytes};
    }

    template <MessageEndpoint EndpointT>
    static auto makeEndpoint(EndpointT endpoint) {
        return endpoint;
    }

private:
    static auto _appendProtocolError(ResponseValues& responses, Id id, std::error_code error) -> bool {
        using ErrorTraits = detail::RpcMethodTraits<void(void)>;
        detail::JsonRpcResponse<ErrorTraits> response;
        response.id    = std::move(id);
        response.error = makeErrorResponse(error);
        auto value     = to_json_value(response);
        if (!value) {
            NEKO_LOG_ERROR("rpc", "serialize JSON-RPC error response failed: {}", value.error().msg);
            return false;
        }
        responses.emplace_back(std::move(value.value()));
        return true;
    }

    static auto makeErrorResponse(std::error_code error) -> detail::JsonRpcErrorResponse {
        return detail::JsonRpcErrorResponse{jsonRpcErrorCode(error), error.message()};
    }

    static auto jsonRpcErrorCode(std::error_code error) -> std::int64_t {
        if (&error.category() != &RpcErrorCategory::instance()) {
            return static_cast<std::int64_t>(error.value());
        }

        switch (static_cast<RpcError>(error.value())) {
        case RpcError::Ok:
            return static_cast<std::int64_t>(JsonRpcError::Ok);
        case RpcError::InvalidRequest:
        case RpcError::MethodIdNotNegotiated:
        case RpcError::MethodTableOutdated:
        case RpcError::MethodIdNotFound:
        case RpcError::MethodIdRemoved:
        case RpcError::MethodSignatureMismatch:
        case RpcError::MethodIdRequiredButUnsupported:
        case RpcError::CompressionRequiredButUnsupported:
            return static_cast<std::int64_t>(JsonRpcError::InvalidRequest);
        case RpcError::Overloaded:
            return static_cast<std::int64_t>(JsonRpcError::Overloaded);
        case RpcError::DeadlineExceeded:
            return static_cast<std::int64_t>(JsonRpcError::DeadlineExceeded);
        case RpcError::MethodNotFound:
            return static_cast<std::int64_t>(JsonRpcError::MethodNotFound);
        case RpcError::InvalidParams:
            return static_cast<std::int64_t>(JsonRpcError::InvalidParams);
        case RpcError::InternalError:
            return static_cast<std::int64_t>(JsonRpcError::InternalError);
        case RpcError::MethodNotBind:
        case RpcError::ClientNotInit:
        case RpcError::ResponseIdNotMatch:
            return static_cast<std::int64_t>(JsonRpcError::ServerErrorStart) -
                   (static_cast<std::int64_t>(error.value()) - 1);
        }
        return static_cast<std::int64_t>(JsonRpcError::InternalError);
    }

    template <typename Method, typename Request, typename... Args>
    static void fillParams(Request& request, Args&&... args) {
        using JsonTraits = detail::JsonRpcMethodTraits<typename Method::MethodTraits>;
        if constexpr (traits::optional_like_type<typename JsonTraits::ParamsTupleType>::value) {
            if constexpr (sizeof...(Args) > 0) {
                request.params = typename JsonTraits::ParamsTupleType(std::forward<Args>(args)...);
            }
        } else if constexpr (JsonTraits::IsAutomaticExpansionAble || JsonTraits::IsTopTuple) {
            if constexpr (sizeof...(Args) > 0) {
                request.params = typename JsonTraits::ParamsTupleType(std::forward<Args>(args)...);
            }
        } else {
            request.params = std::make_tuple(std::forward<Args>(args)...);
        }
    }
};

template <typename... Protocols>
using JsonRpcServer = RpcServer<JsonRpcBackend, Protocols...>;

template <typename... Protocols>
using JsonRpcClient = RpcClient<JsonRpcBackend, Protocols...>;

NEKO_END_NAMESPACE
