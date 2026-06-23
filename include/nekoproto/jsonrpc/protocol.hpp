#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "nekoproto/rpc/method.hpp"
#include "nekoproto/jsonrpc/jsonrpc_traits.hpp"
#include "nekoproto/serialization/json_serializer.hpp"
#include "nekoproto/serialization/parsing/basic.hpp"
#include "nekoproto/serialization/parsing/map.hpp"
#include "nekoproto/serialization/parsing/pointer.hpp"
#include "nekoproto/serialization/parsing/reflection.hpp"
#include "nekoproto/serialization/parsing/sequence.hpp"
#include "nekoproto/serialization/parsing/tuple.hpp"
#include "nekoproto/serialization/parsing/utility.hpp"
#include "nekoproto/serialization/parsing/variant.hpp"

NEKO_BEGIN_NAMESPACE
namespace detail {

using JsonRpcIdType = std::variant<std::monostate, uint64_t, std::string>;
using JsonRpcResponseValues = std::vector<JsonSerializer::JsonValue>;

NEKO_PROTO_API std::string to_string(JsonRpcIdType id);

template <typename MethodTraits, ConstexprString... ArgNames>
struct JsonRpcRequest2 {
    using JsonTraits = JsonRpcMethodTraits<MethodTraits>;
    using ParamsTupleType = typename JsonTraits::ParamsTupleType;
    static_assert(sizeof...(ArgNames) == 0 || JsonTraits::NumParams == sizeof...(ArgNames),
                  "JsonRpcRequest2: The number of parameters and names do not match.");
    static_assert((is_std_tuple_v<ParamsTupleType> ? true : (sizeof...(ArgNames) == 0)),
                  "JsonRpcRequest2: ParamsTupleType must be tuple if argnames > 0 and not tuple if argnames == 0.");

    std::optional<std::string> jsonrpc = "2.0";
    std::string method;
    ParamsTupleType params;
    JsonRpcIdType id;

    NEKO_SERIALIZER(jsonrpc, method, params, id)
};

template <typename R, typename W, typename T, ConstexprString... ArgNames>
struct disable_reflect_parser<R, W, JsonRpcSerializerHelperObject<T, ArgNames...>>
    : std::true_type {};

template <typename R, typename W, typename MethodTraits, ConstexprString... ArgNames>
struct disable_reflect_parser<R, W, JsonRpcRequest2<MethodTraits, ArgNames...>>
    : std::true_type {};

template <typename R, typename W, typename T, ConstexprString... ArgNames>
struct Parser<R, W, JsonRpcSerializerHelperObject<T, ArgNames...>, void> {
    using Helper = JsonRpcSerializerHelperObject<T, ArgNames...>;

    template <std::size_t... Is>
    static ParserResult writeObject(W& writer, typename W::OutputObjectType& object, const Helper& value,
                                    std::index_sequence<Is...>) {
        constexpr std::array<std::string_view, sizeof...(ArgNames)> argNames = {ArgNames.view()...};
        ParserResult result;
        const auto writeField = [&]<std::size_t I>() {
            if (result) {
                result = parser_write_reflect_field<R, W>(writer, object, std::get<I>(value.mTuple), argNames[I],
                                                          NoTags{});
            }
        };
        (writeField.template operator()<Is>(), ...);
        return result;
    }

    template <std::size_t... Is>
    static ParserResult readObject(typename R::InputValueType in, Helper& value, std::index_sequence<Is...>) {
        constexpr std::array<std::string_view, sizeof...(ArgNames)> argNames = {ArgNames.view()...};
        ParserResult result;
        const auto readField = [&]<std::size_t I>() {
            if (result) {
                result = parser_read_reflect_field<R, W>(in, std::get<I>(value.mTuple), argNames[I], NoTags{});
            }
        };
        (readField.template operator()<Is>(), ...);
        return result;
    }

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Helper& value, const ParentType& parent, const Tags& tags) {
        if constexpr (sizeof...(ArgNames) > 0) {
            auto object = parsing::Parent<W>::addObject(writer, sizeof...(ArgNames), parent);
            return writeObject(writer, object, value, std::make_index_sequence<sizeof...(ArgNames)>{});
        } else {
            return parser_write<R, W>(writer, value.mTuple, parent, tags);
        }
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Helper& value, const Tags& tags) {
        if constexpr (sizeof...(ArgNames) > 0) {
            auto object = R::toObject(in);
            if (object) {
                return readObject(in, value, std::make_index_sequence<sizeof...(ArgNames)>{});
            }
        }
        return parser_read<R, W>(in, value.mTuple, tags);
    }
};

template <typename R, typename W, typename MethodTraits, ConstexprString... ArgNames>
struct Parser<R, W, JsonRpcRequest2<MethodTraits, ArgNames...>, void> {
    using Request = JsonRpcRequest2<MethodTraits, ArgNames...>;

    template <typename ParentType, typename Tags>
    static ParserResult write(W& writer, const Request& value, const ParentType& parent, const Tags& /*tags*/) {
        auto object = parsing::Parent<W>::addObject(writer, 4, parent);
        auto result = parser_write_reflect_field<R, W>(writer, object, value.jsonrpc, "jsonrpc", NoTags{});
        if (!result) {
            return result;
        }
        result = parser_write_reflect_field<R, W>(writer, object, value.method, "method", NoTags{});
        if (!result) {
            return result;
        }
        result = parser_write_reflect_field<R, W>(writer, object, value.id, "id", NoTags{});
        if (!result) {
            return result;
        }
        if constexpr (sizeof...(ArgNames) == 0) {
            result = parser_write_reflect_field<R, W>(writer, object, value.params, "params", NoTags{});
        } else if constexpr (Request::JsonTraits::NumParams > 0 && Request::JsonTraits::ParamsSize > 0) {
            JsonRpcSerializerHelperObject<const typename Request::ParamsTupleType, ArgNames...> paramsHelper(
                value.params);
            result = parser_write_reflect_field<R, W>(writer, object, paramsHelper, "params", NoTags{});
        }
        return result;
    }

    template <typename Tags>
    static ParserResult read(typename R::InputValueType in, Request& value, const Tags& /*tags*/) {
        auto object = R::toObject(in);
        if (!object) {
            return object.error();
        }
        auto result = parser_read_reflect_field<R, W>(in, value.jsonrpc, "jsonrpc", NoTags{});
        if (!result) {
            return result;
        }
        result = parser_read_reflect_field<R, W>(in, value.method, "method", NoTags{});
        if (!result) {
            return result;
        }
        result = parser_read_reflect_field<R, W>(in, value.id, "id", NoTags{});
        if (!result) {
            return result;
        }
        if constexpr (sizeof...(ArgNames) == 0) {
            result = parser_read_reflect_field<R, W>(in, value.params, "params", NoTags{});
        } else {
            JsonRpcSerializerHelperObject<typename Request::ParamsTupleType, ArgNames...> paramsHelper(value.params);
            result = parser_read_reflect_field<R, W>(in, paramsHelper, "params", NoTags{});
        }
        if (!result && Request::JsonTraits::IsNullAble &&
            result.error().ec == sa::make_error_code(sa::ErrorCode::InvalidField)) {
            return sa::success();
        }
        return result;
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

NEKO_END_NAMESPACE
