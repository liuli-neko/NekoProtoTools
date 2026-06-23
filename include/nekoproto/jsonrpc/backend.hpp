#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

#include "ilias/io/error.hpp"
#include "ilias/result.hpp"
#include "nekoproto/jsonrpc/jsonrpc_error.hpp"
#include "nekoproto/jsonrpc/protocol.hpp"
#include "nekoproto/rpc/client.hpp"
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
        bool ok = false;
        bool batch = false;
        std::vector<DecodedRequest> requests;
    };

    struct EncodedRequest {
        Message message;
        Id id;
    };

    static DecodeResult decodeIncoming(std::span<const std::byte> message) {
        DecodeResult result;
        auto data = reinterpret_cast<const char*>(message.data());
        auto size = message.size();
        while (size > 0 && (*data == '\n' || *data == ' ')) {
            ++data;
            --size;
        }

        JsonSerializer::JsonValue root;
        JsonSerializer::InputSerializer rootIn(data, size);
        if (!rootIn(root)) {
            return result;
        }

        result.ok = true;
        result.batch = root.isArray();
        std::vector<JsonSerializer::JsonValue> rawRequests;
        if (result.batch) {
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
            if (!methodIn(method)) {
                break;
            }
            if (method.jsonrpc.has_value() && method.jsonrpc.value() != "2.0") {
                break;
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
    static auto decodeParams(const DecodedRequest& request)
        -> ilias::Result<typename detail::JsonRpcMethodTraits<typename Method::MethodTraits>::ParamsTupleType,
                         std::error_code> {
        using Request = typename Method::template ApplyArgNames<detail::JsonRpcRequest2>;
        Request decoded;
        JsonSerializer::InputSerializer in(request.value);
        if (in(decoded)) {
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
        Response response;
        response.id = request.method.id;
        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            response.result = nullptr;
        } else {
            response.result = std::forward<RetT>(ret);
        }
        auto value = to_json_value(response);
        if (value.hasValue()) {
            responses.emplace_back(std::move(value));
        }
    }

    static void appendError(ResponseValues& responses, const DecodedRequest& request, std::error_code error) {
        if (!expectsResponse(request)) {
            return;
        }
        using ErrorTraits = detail::RpcMethodTraits<void(void)>;
        detail::JsonRpcResponse<ErrorTraits> response;
        response.id = request.method.id;
        response.error = detail::JsonRpcErrorResponse{static_cast<int64_t>(error.value()), error.message()};
        auto value = to_json_value(response);
        if (value.hasValue()) {
            responses.emplace_back(std::move(value));
        }
    }

    static Message encodeResponses(const ResponseValues& responses, bool batch) {
        Message buffer;
        if (responses.empty()) {
            return buffer;
        }
        JsonSerializer::OutputSerializer out(buffer);
        if (batch) {
            out(responses);
        } else {
            out(responses.front());
        }
        out.end();
        return buffer;
    }

    template <typename Method, typename... Args>
    static auto encodeRequest(Method& method, bool notification, std::uint64_t& nextId, Args&&... args)
        -> ilias::Result<EncodedRequest, std::error_code> {
        using Request = typename Method::template ApplyArgNames<detail::JsonRpcRequest2>;
        Request request;
        request.method = std::string(method.name());
        if (notification) {
            request.id = std::monostate{};
        } else {
            request.id = nextId++;
        }
        fillParams<Method>(request, std::forward<Args>(args)...);

        Message buffer;
        JsonSerializer::OutputSerializer out(buffer);
        if (out(request) && out.end()) {
            return EncodedRequest{.message = std::move(buffer), .id = request.id};
        }
        return ilias::Err(JsonRpcError::InvalidRequest);
    }

    template <typename Method>
    static auto decodeResponse(std::span<const std::byte> buffer, const Id& expectedId)
        -> ilias::Result<typename Method::RawReturnType, std::error_code> {
        using Response = detail::JsonRpcResponse<typename Method::MethodTraits>;
        Response response;
        JsonSerializer::InputSerializer in(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (!in(response)) {
            return ilias::Err(JsonRpcError::ParseError);
        }
        if (response.id != expectedId) {
            return ilias::Err(JsonRpcError::ResponseIdNotMatch);
        }
        if (response.error.has_value()) {
            auto err = response.error.value();
            return ilias::Err(err.code > 0 ? std::error_code(ilias::IoError::Code(err.code))
                                           : std::error_code(JsonRpcError(err.code)));
        }
        if constexpr (std::is_void_v<typename Method::RawReturnType>) {
            return {};
        } else {
            return response.result.value();
        }
    }

    static std::error_code clientNotInitError() { return JsonRpcError::ClientNotInit; }
    static std::error_code notificationOk() { return JsonRpcError::Ok; }

private:
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
