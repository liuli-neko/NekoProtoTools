#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "nekoproto/jsonrpc/jsonrpc_traits.hpp"
#include "nekoproto/rpc/method.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/parsing/reflection.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"

namespace nekoproto {
namespace detail {

using JsonRpcIdType         = std::variant<std::monostate, uint64_t, std::string>;
using JsonRpcResponseValues = std::vector<JsonSerializer::JsonValue>;

// JSON-RPC IDs are protocol scalars, not the generic tagged representation
// used for application variants.
template <typename W>
struct WriteParser<W, JsonRpcIdType, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const JsonRpcIdType& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return std::visit(
            [&](const auto& active) -> ParserResult { return parserWrite<W>(writer, active, parent, tags); }, value);
    }
};

template <typename R>
struct ReadParser<R, JsonRpcIdType, void> {
    template <typename Tags>
    static auto read(typename R::InputValueType in, JsonRpcIdType& value, const Tags& tags) -> ParserResult {
        if (R::isEmpty(in)) {
            value = std::monostate{};
            return sa::success();
        }
        std::uint64_t integer = 0;
        if (auto result = parserRead<R>(in, integer, tags); result) {
            value = integer;
            return result;
        }
        std::string string;
        if (auto result = parserRead<R>(in, string, tags); result) {
            value = std::move(string);
            return result;
        }
        return parserError(sa::ErrorCode::InvalidType, "JSON-RPC id must be null, an unsigned integer, or a string");
    }
};

template <typename MethodTraits>
struct JsonRpcRequest2 {
    using JsonTraits      = JsonRpcMethodTraits<MethodTraits>;
    using ParamsTupleType = typename JsonTraits::ParamsTupleType;

    std::optional<std::string> jsonrpc = "2.0";
    std::string method;
    ParamsTupleType params;
    JsonRpcIdType id;

    NEKO_SERIALIZER(jsonrpc, method, params, id)
};

template <typename T>
struct JsonRpcRequestWithContext {
    T& request;
    JsonRpcMethodContext context{};
};

template <typename T>
struct DisableReflectParser<JsonRpcSerializerHelperObject<T>> : std::true_type {};

template <typename MethodTraits>
struct DisableReflectParser<JsonRpcRequest2<MethodTraits>> : std::true_type {};

template <typename T>
struct DisableReflectParser<JsonRpcRequestWithContext<T>> : std::true_type {};

template <typename W, typename T>
struct WriteParser<W, JsonRpcSerializerHelperObject<T>, void> {
    using Helper = JsonRpcSerializerHelperObject<T>;
    using Tuple  = std::decay_t<T>;

    template <std::size_t... Is>
    static auto writeObject(W& writer, typename W::OutputObjectType& object, const Helper& value,
                                    std::index_sequence<Is...>) -> ParserResult {
        ParserResult result;
        const auto writeField = [&]<std::size_t I>() {
            if (result) {
                result = parserWriteReflectField<W>(writer, object, std::get<I>(value.mTuple),
                                                       value.context.argNames[I], NoTags{});
            }
        };
        (writeField.template operator()<Is>(), ...);
        return result;
    }

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Helper& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        if constexpr (is_std_tuple_v<Tuple>) {
            constexpr auto tupleSize = std::tuple_size_v<Tuple>;
            if (!value.context.argNames.empty()) {
                if (value.context.argNames.size() != tupleSize) {
                    return parserError(sa::ErrorCode::InvalidLength,
                                        "Named JSON-RPC params count does not match tuple size");
                }
                auto object = parsing::Parent<W>::addObject(writer, value.context.argNames.size(), parent);
                return writeObject(writer, object, value, std::make_index_sequence<tupleSize>{});
            }
        } else if (!value.context.argNames.empty()) {
            return parserError(sa::ErrorCode::InvalidType, "Named JSON-RPC params require tuple parameters");
        }
        return parserWrite<W>(writer, value.mTuple, parent, tags);
    }
};

template <typename R, typename T>
struct ReadParser<R, JsonRpcSerializerHelperObject<T>, void> {
    using Helper = JsonRpcSerializerHelperObject<T>;
    using Tuple  = std::decay_t<T>;

    template <std::size_t... Is>
    static auto readObject(typename R::InputValueType in, Helper& value, std::index_sequence<Is...>) -> ParserResult {
        ParserResult result;
        const auto readField = [&]<std::size_t I>() {
            if (result) {
                result =
                    parserReadReflectField<R>(in, std::get<I>(value.mTuple), value.context.argNames[I], NoTags{});
            }
        };
        (readField.template operator()<Is>(), ...);
        return result;
    }

    template <typename Tags>
    static auto read(typename R::InputValueType in, Helper& value, const Tags& tags) -> ParserResult {
        if constexpr (is_std_tuple_v<Tuple>) {
            constexpr auto tupleSize = std::tuple_size_v<Tuple>;
            if (!value.context.argNames.empty()) {
                if (value.context.argNames.size() != tupleSize) {
                    return parserError(sa::ErrorCode::InvalidLength,
                                        "Named JSON-RPC params count does not match tuple size");
                }
                auto object = R::toObject(in);
                if (object) {
                    return readObject(in, value, std::make_index_sequence<tupleSize>{});
                }
            }
        } else if (!value.context.argNames.empty()) {
            return parserError(sa::ErrorCode::InvalidType, "Named JSON-RPC params require tuple parameters");
        }
        return parserRead<R>(in, value.mTuple, tags);
    }
};

template <typename MethodTraits>
struct JsonRpcNamedParams {
    static auto provided(const JsonRpcMethodContext& context) noexcept -> bool { return !context.argNames.empty(); }

    static auto matchesParamsSize(const JsonRpcMethodContext& context) noexcept -> bool {
        return !context.argNames.empty() &&
               context.argNames.size() == static_cast<std::size_t>(JsonRpcMethodTraits<MethodTraits>::ParamsSize);
    }
};

template <typename MethodTraits>
struct JsonRpcRequestParser {
    using Request = JsonRpcRequest2<MethodTraits>;

    template <typename W, typename ParentType, typename Tags>
    static auto write(W& writer, const Request& value, const JsonRpcMethodContext& context,
                              const ParentType& parent, const Tags& /*tags*/) -> ParserResult {
        auto object = parsing::Parent<W>::addObject(writer, 4, parent);
        auto result = parserWriteReflectField<W>(writer, object, value.jsonrpc, "jsonrpc", NoTags{});
        if (!result) {
            return result;
        }
        result = parserWriteReflectField<W>(writer, object, value.method, "method", NoTags{});
        if (!result) {
            return result;
        }
        result = parserWriteReflectField<W>(writer, object, value.id, "id", NoTags{});
        if (!result) {
            return result;
        }
        if (JsonRpcNamedParams<MethodTraits>::provided(context)) {
            if (!JsonRpcNamedParams<MethodTraits>::matchesParamsSize(context)) {
                return parserError(sa::ErrorCode::InvalidLength,
                                    "Named JSON-RPC params count does not match params size");
            }
            if constexpr (Request::JsonTraits::ParamsSize > 0 && is_std_tuple_v<typename Request::ParamsTupleType>) {
                JsonRpcSerializerHelperObject<const typename Request::ParamsTupleType> paramsHelper(value.params,
                                                                                                     context);
                result = parserWriteReflectField<W>(writer, object, paramsHelper, "params", NoTags{});
            } else {
                return parserError(sa::ErrorCode::InvalidType, "Named JSON-RPC params require tuple parameters");
            }
        } else {
            result = parserWriteReflectField<W>(writer, object, value.params, "params", NoTags{});
        }
        return result;
    }

    template <typename R, typename Tags>
    static auto read(typename R::InputValueType in, Request& value, const JsonRpcMethodContext& context,
                             const Tags& /*tags*/) -> ParserResult {
        auto object = R::toObject(in);
        if (!object) {
            return object.error();
        }
        auto result = parserReadReflectField<R>(in, value.jsonrpc, "jsonrpc", NoTags{});
        if (!result) {
            return result;
        }
        result = parserReadReflectField<R>(in, value.method, "method", NoTags{});
        if (!result) {
            return result;
        }
        result = parserReadReflectField<R>(in, value.id, "id", NoTags{});
        if (!result) {
            return result;
        }
        if (JsonRpcNamedParams<MethodTraits>::provided(context)) {
            if (!JsonRpcNamedParams<MethodTraits>::matchesParamsSize(context)) {
                return parserError(sa::ErrorCode::InvalidLength,
                                    "Named JSON-RPC params count does not match params size");
            }
            if constexpr (Request::JsonTraits::ParamsSize > 0 && is_std_tuple_v<typename Request::ParamsTupleType>) {
                JsonRpcSerializerHelperObject<typename Request::ParamsTupleType> params_helper(value.params, context);
                result = parserReadReflectField<R>(in, params_helper, "params", NoTags{});
            } else {
                return parserError(sa::ErrorCode::InvalidType, "Named JSON-RPC params require tuple parameters");
            }
        } else {
            result = parserReadReflectField<R>(in, value.params, "params", NoTags{});
        }
        if (!result && Request::JsonTraits::IsNullAble &&
            result.error().ec == sa::makeErrorCode(sa::ErrorCode::InvalidField)) {
            return sa::success();
        }
        return result;
    }
};

template <typename W, typename MethodTraits>
struct WriteParser<W, JsonRpcRequest2<MethodTraits>, void> {
    using Request = JsonRpcRequest2<MethodTraits>;

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Request& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<MethodTraits>::template write<W>(writer, value, {}, parent, tags);
    }
};

template <typename R, typename MethodTraits>
struct ReadParser<R, JsonRpcRequest2<MethodTraits>, void> {
    using Request = JsonRpcRequest2<MethodTraits>;

    template <typename Tags>
    static auto read(typename R::InputValueType in, Request& value, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<MethodTraits>::template read<R>(in, value, {}, tags);
    }
};

template <typename W, typename MethodTraits>
struct WriteParser<W, JsonRpcRequestWithContext<JsonRpcRequest2<MethodTraits>>, void> {
    using Wrapped = JsonRpcRequestWithContext<JsonRpcRequest2<MethodTraits>>;

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Wrapped& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<MethodTraits>::template write<W>(writer, value.request, value.context, parent, tags);
    }
};

template <typename R, typename MethodTraits>
struct ReadParser<R, JsonRpcRequestWithContext<JsonRpcRequest2<MethodTraits>>, void> {
    using Wrapped = JsonRpcRequestWithContext<JsonRpcRequest2<MethodTraits>>;

    template <typename Tags>
    static auto read(typename R::InputValueType in, Wrapped& value, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<MethodTraits>::template read<R>(in, value.request, value.context, tags);
    }
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

template <typename MethodTraits>
struct JsonRpcResponse {
    using ReturnType = typename JsonRpcMethodTraits<MethodTraits>::ReturnType;

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

} // namespace detail

} // namespace nekoproto
