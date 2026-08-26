#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "nekoproto/jsonrpc/jsonrpc_traits.hpp"
#include "nekoproto/rpc/method.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/parsing/parsers.hpp"
#include "nekoproto/serialization/parsing/reflection.hpp"

namespace nekoproto {
namespace detail {

using JsonRpcIdType         = std::variant<std::monostate, uint64_t, std::string>;
using JsonRpcResponseValues = std::vector<JsonSerializer::JsonValue>;

// JSON-RPC IDs are protocol scalars, not the generic tagged representation
// used for application variants.
template <typename W>
struct WriteParser<W, JsonRpcIdType, void> {
    template <typename ParentType, typename Tags>
    static auto write(W& writer, const JsonRpcIdType& value, const ParentType& parent, const Tags& tags)
        -> ParserResult {
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
        return makeParserError(sa::ErrorCode::InvalidType,
                               "JSON-RPC id must be null, an unsigned integer, or a string");
    }
};

template <typename ParamsTupleType>
struct JsonRpcRequestPayload {
    std::optional<std::string> jsonrpc = "2.0";
    std::string method;
    ParamsTupleType params;
    JsonRpcIdType id;
};

template <typename MethodTraits>
using JsonRpcRequest2 = JsonRpcRequestPayload<typename JsonRpcMethodTraits<MethodTraits>::ParamsTupleType>;

template <typename T>
struct JsonRpcRequestWithContext {
    T& request;
    JsonRpcMethodContext context{};
};

template <typename T>
struct DisableReflectParser<JsonRpcSerializerHelperObject<T>> : std::true_type {};

template <typename ParamsTupleType>
struct DisableReflectParser<JsonRpcRequestPayload<ParamsTupleType>> : std::true_type {};

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
                result = parserWriteReflectField<W>(writer, object, std::get<I>(value.tuple),
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
                    return makeParserError(sa::ErrorCode::InvalidLength,
                                           "Named JSON-RPC params count does not match tuple size");
                }
                auto object = parsing::Parent<W>::addObject(writer, value.context.argNames.size(), parent);
                return writeObject(writer, object, value, std::make_index_sequence<tupleSize>{});
            }
        } else if (!value.context.argNames.empty()) {
            return makeParserError(sa::ErrorCode::InvalidType, "Named JSON-RPC params require tuple parameters");
        }
        return parserWrite<W>(writer, value.tuple, parent, tags);
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
                result = parserReadReflectField<R>(in, std::get<I>(value.tuple), value.context.argNames[I], NoTags{});
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
                    return makeParserError(sa::ErrorCode::InvalidLength,
                                           "Named JSON-RPC params count does not match tuple size");
                }
                auto object = R::toObject(in);
                if (object) {
                    return readObject(in, value, std::make_index_sequence<tupleSize>{});
                }
            }
        } else if (!value.context.argNames.empty()) {
            return makeParserError(sa::ErrorCode::InvalidType, "Named JSON-RPC params require tuple parameters");
        }
        return parserRead<R>(in, value.tuple, tags);
    }
};

template <typename T>
consteval auto jsonRpcParamsSize() -> std::size_t {
    if constexpr (is_std_tuple_v<T>) {
        return std::tuple_size_v<T>;
    } else {
        return 1;
    }
}

template <typename ParamsTupleType>
struct JsonRpcNamedParams {
    static constexpr std::size_t ParamsSize = jsonRpcParamsSize<ParamsTupleType>();

    static auto provided(const JsonRpcMethodContext& context) noexcept -> bool { return !context.argNames.empty(); }

    static auto matchesParamsSize(const JsonRpcMethodContext& context) noexcept -> bool {
        return !context.argNames.empty() && context.argNames.size() == ParamsSize;
    }
};

template <typename ParamsTupleType>
struct JsonRpcRequestParser {
    using Request                           = JsonRpcRequestPayload<ParamsTupleType>;
    static constexpr std::size_t ParamsSize = jsonRpcParamsSize<ParamsTupleType>();
    static constexpr bool IsNullAble        = jsonrpcIsNullAbleObject<ParamsTupleType>();

    template <typename W, typename ParentType, typename Tags>
    static auto write(W& writer, const Request& value, const JsonRpcMethodContext& context, const ParentType& parent,
                      const Tags& /*tags*/) -> ParserResult {
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
        if (JsonRpcNamedParams<ParamsTupleType>::provided(context)) {
            if (!JsonRpcNamedParams<ParamsTupleType>::matchesParamsSize(context)) {
                return makeParserError(sa::ErrorCode::InvalidLength,
                                       "Named JSON-RPC params count does not match params size");
            }
            if constexpr (ParamsSize > 0 && is_std_tuple_v<ParamsTupleType>) {
                JsonRpcSerializerHelperObject<const ParamsTupleType> paramsHelper(value.params, context);
                result = parserWriteReflectField<W>(writer, object, paramsHelper, "params", NoTags{});
            } else {
                return makeParserError(sa::ErrorCode::InvalidType, "Named JSON-RPC params require tuple parameters");
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
        if (JsonRpcNamedParams<ParamsTupleType>::provided(context)) {
            if (!JsonRpcNamedParams<ParamsTupleType>::matchesParamsSize(context)) {
                return makeParserError(sa::ErrorCode::InvalidLength,
                                       "Named JSON-RPC params count does not match params size");
            }
            if constexpr (ParamsSize > 0 && is_std_tuple_v<ParamsTupleType>) {
                JsonRpcSerializerHelperObject<ParamsTupleType> params_helper(value.params, context);
                result = parserReadReflectField<R>(in, params_helper, "params", NoTags{});
            } else {
                return makeParserError(sa::ErrorCode::InvalidType, "Named JSON-RPC params require tuple parameters");
            }
        } else {
            result = parserReadReflectField<R>(in, value.params, "params", NoTags{});
        }
        if (!result && IsNullAble &&
            result.error().ec == sa::makeErrorCode(sa::ErrorCode::InvalidField)) {
            return sa::success();
        }
        return result;
    }
};

template <typename W, typename ParamsTupleType>
struct WriteParser<W, JsonRpcRequestPayload<ParamsTupleType>, void> {
    using Request = JsonRpcRequestPayload<ParamsTupleType>;

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Request& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<ParamsTupleType>::template write<W>(writer, value, {}, parent, tags);
    }
};

template <typename R, typename ParamsTupleType>
struct ReadParser<R, JsonRpcRequestPayload<ParamsTupleType>, void> {
    using Request = JsonRpcRequestPayload<ParamsTupleType>;

    template <typename Tags>
    static auto read(typename R::InputValueType in, Request& value, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<ParamsTupleType>::template read<R>(in, value, {}, tags);
    }
};

template <typename W, typename ParamsTupleType>
struct WriteParser<W, JsonRpcRequestWithContext<JsonRpcRequestPayload<ParamsTupleType>>, void> {
    using Wrapped = JsonRpcRequestWithContext<JsonRpcRequestPayload<ParamsTupleType>>;

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Wrapped& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<ParamsTupleType>::template write<W>(writer, value.request, value.context, parent,
                                                                        tags);
    }
};

template <typename W, typename ParamsTupleType>
struct WriteParser<W, JsonRpcRequestWithContext<const JsonRpcRequestPayload<ParamsTupleType>>, void> {
    using Wrapped = JsonRpcRequestWithContext<const JsonRpcRequestPayload<ParamsTupleType>>;

    template <typename ParentType, typename Tags>
    static auto write(W& writer, const Wrapped& value, const ParentType& parent, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<ParamsTupleType>::template write<W>(writer, value.request, value.context, parent,
                                                                        tags);
    }
};

template <typename R, typename ParamsTupleType>
struct ReadParser<R, JsonRpcRequestWithContext<JsonRpcRequestPayload<ParamsTupleType>>, void> {
    using Wrapped = JsonRpcRequestWithContext<JsonRpcRequestPayload<ParamsTupleType>>;

    template <typename Tags>
    static auto read(typename R::InputValueType in, Wrapped& value, const Tags& tags) -> ParserResult {
        return JsonRpcRequestParser<ParamsTupleType>::template read<R>(in, value.request, value.context, tags);
    }
};

struct JsonRpcRequestMethod {
    std::optional<std::string> jsonrpc;
    std::string method;
    JsonRpcIdType id;

    // clang-format off
    struct Neko {
        static constexpr auto value = Object(
            "jsonrpc", &JsonRpcRequestMethod::jsonrpc, 
            "method",  &JsonRpcRequestMethod::method,
            "id",      &JsonRpcRequestMethod::id);
    };
    // clang-format on
};

struct JsonRpcErrorResponse {
    int64_t code;
    std::string message;
    // clang-format off
    struct Neko {
        static constexpr auto value = Object(
            "code",    &JsonRpcErrorResponse::code, 
            "message",  &JsonRpcErrorResponse::message);
    };
    // clang-format on
};

template <typename ResultType>
struct JsonRpcResponsePayload {
    std::string jsonrpc = "2.0";
    ResultType result;
    std::optional<JsonRpcErrorResponse> error;
    JsonRpcIdType id;

    // clang-format off
    struct Neko {
        static constexpr auto value = Object(
            "jsonrpc", &JsonRpcResponsePayload::jsonrpc, 
            "result",  &JsonRpcResponsePayload::result,
            "error",   &JsonRpcResponsePayload::error,
            "id",      &JsonRpcResponsePayload::id);
    };
    // clang-format on
};

template <>
struct JsonRpcResponsePayload<void> {
    std::string jsonrpc = "2.0";
    std::optional<JsonRpcErrorResponse> error;
    JsonRpcIdType id;

    // clang-format off
    struct Neko {
        static constexpr auto value = Object(
            "jsonrpc", &JsonRpcResponsePayload<void>::jsonrpc, 
            "error",   &JsonRpcResponsePayload<void>::error,
            "id",      &JsonRpcResponsePayload<void>::id);
    };
    // clang-format on
};

template <typename T>
struct JsonRpcResponseHelper {
    using type = JsonRpcResponsePayload<typename JsonRpcMethodTraits<T>::ReturnType>;
};

template <>
struct JsonRpcResponseHelper<void> {
    using type = JsonRpcResponsePayload<void>;
};

template <typename T>
using JsonRpcResponse = typename JsonRpcResponseHelper<T>::type;

} // namespace detail

} // namespace nekoproto
